
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include "UdpService.h"




using namespace muduo;
using namespace muduo::net;





UdpService::UdpService(EventLoop* loop, const InetAddress& localaddr)
    : loop_(CHECK_NOTNULL(loop)),
      udpMsgRecvThread_(new muduo::Thread(boost::bind(&UdpService::runInUdpMsgRecvThread, this))),
      udpMsgSendThread_(new muduo::Thread(boost::bind(&UdpService::runInUdpMsgSendThread, this))),
      mutex_(),
      notEmpty(mutex_),
      udpSocket_(localaddr.family() == PF_INET ? UdpSocket::IPV4 : UdpSocket::IPV6),
      time_(Timestamp::now()),
      count_(0),
      running_(false) {

    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetRecvBuf(512*1024);
    udpSocket_.SetSendBuf(512*1024);
    udpSocket_.SetTosWithLowDelay();
    udpSocket_.SetRecvTimeout(60000);
    udpSocket_.SetSendTimeout(60000);

    udpSocket_.BindAddress(localaddr);
}


UdpService::~UdpService() {
    loop_->assertInLoopThread();
    Stop();
}

void UdpService::Start() {
    if (running_) {
        return;
    }

    running_ = true;


    udpMsgSendThread_->start();
    udpMsgRecvThread_->start();

    loop_->runEvery(1.0, boost::bind(&UdpService::messagePerSecond, this));

    LOG_TRACE << "UdpService::Start";
}

void UdpService::Stop() {
    if (loop_->isInLoopThread()) {
        stopInLoop();
    } else {
        loop_->runInLoop(boost::bind(&UdpService::stopInLoop, this));
    }
}

void UdpService::stopInLoop() {
    if (!running_) {
        return;
    }

    running_ = false;

    notEmpty.notifyAll();

    udpMsgRecvThread_->join();
    udpMsgSendThread_->join();
    LOG_TRACE << "UdpService::stopInLoop";
}

void UdpService::runInUdpMsgRecvThread() {
    while (running_) {
         int ret = 0;
         UdpMessagePtr message;

         boost::tie(ret, message) =  udpSocket_.RecvMsg();

         if (ret >= 0) {
             loop_->runInLoop(boost::bind(&UdpService::messageInloop, this, message));
         }
    }
    LOG_TRACE << "UdpMsgRecvThread Stop";
}

void UdpService::messageInloop(UdpMessagePtr msg) {
    count_++;
    if (udpMessageCallback_) {
        udpMessageCallback_(msg);
    }
}

void UdpService::runInUdpMsgSendThread() {
    while(running_) {
        UdpMessagePtr message;

        {
            MutexLockGuard lock(mutex_);

            while(queue_.empty() && running_) {
                notEmpty.wait();
            }

            if (!queue_.empty()) {
                message = queue_.front();
                queue_.pop_front();
            }

        }

        if (message) {
            udpSocket_.SendMsg(message);
        }

    }

    LOG_TRACE << "UdpMsgSendThread stop";
}

void UdpService::SendMsg(UdpMessagePtr& msg) {
    MutexLockGuard lock(mutex_);
    queue_.push_back(msg);
    notEmpty.notify();
}


void UdpService::messagePerSecond() {
    Timestamp now = Timestamp::now();
    LOG_INFO << static_cast<double>(count_)/(muduo::timeDifference(now, time_)) <<  " message persecond";
    time_ = now;
    count_ = 0;
}
