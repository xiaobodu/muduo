

#include <stddef.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/SocketsOps.h>
#ifdef PERFORMANCE_MONITOR
#include <gperftools/profiler.h>
#endif
#include "UdpService.h"
#include <sys/errno.h>




using namespace muduo;
using namespace muduo::net;





UdpService::UdpService(EventLoop* loop, const InetAddress& localaddr)
    : loop_(CHECK_NOTNULL(loop)),
      udpMsgRecvThread_(new muduo::Thread(boost::bind(&UdpService::runInUdpMsgRecvThread, this), "UdpRecv")),
      udpMsgSendThread_(new muduo::Thread(boost::bind(&UdpService::runInUdpMsgSendThread, this), "UdpSend")),
      mutex_(),
      notEmpty(mutex_),
      udpSocket_(localaddr.family() == PF_INET ? UdpSocket::IPV4 : UdpSocket::IPV6),
      time_(Timestamp::now()),
      count_(0),
      running_(false) {

    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetRecvBuf(2*1024*1024);
    udpSocket_.SetSendBuf(16*1024*1024);
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
#ifdef PERFORMANCE_MONITOR
    loop_->runEvery(1.0, boost::bind(&UdpService::messagePerSecond, this));
#endif

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
#ifdef PERFORMANCE_MONITOR
    ProfilerRegisterThread();
#endif
    while (running_) {
         const size_t initialSize = 1472;
         struct sockaddr fromAddr;
         ::bzero(&fromAddr, sizeof(fromAddr));
         socklen_t addrLen = sizeof(fromAddr);

         boost::shared_ptr<Buffer> recvBuffer = boost::make_shared<Buffer>(initialSize);

         ssize_t nr = ::recvfrom(udpSocket_.sockfd(),
                                 recvBuffer->beginWrite(),
                                 recvBuffer->writableBytes(),
                                 0,
                                 &fromAddr,
                                 &addrLen);

         if (nr >= 0) {
            recvBuffer->hasWritten(nr);
            InetAddress intetAddress;
            intetAddress.setSockAddrInet6(*sockets::sockaddr_in6_cast(&fromAddr));
            loop_->queueInLoop(boost::bind(&UdpService::messageInloop, this, boost::make_shared<UdpMessage>(recvBuffer, intetAddress)));
            LOG_TRACE << "recvfrom return, readn = " << nr << " from " << intetAddress.toIpPort();
         } else {

             if (errno != EAGAIN &&
                 errno != EWOULDBLOCK &&
                 errno != EINTR) {
                 LOG_SYSERR << "recvfrom return";
             }
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
#ifdef PERFORMANCE_MONITOR
    ProfilerRegisterThread();
#endif
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

            ssize_t nw = ::sendto(udpSocket_.sockfd(),
                                  message->buffer()->peek(),
                                  message->buffer()->readableBytes(),
                                  0,
                                  message->intetAddress().getSockAddr(),
                                  sizeof(struct sockaddr_in6));
            if (nw >= 0) {
                message->buffer()->retrieve(nw);
            } else {
                LOG_SYSERR << "sendto failed";
            }
        }

    }

    LOG_TRACE << "UdpMsgSendThread stop";
}

void UdpService::SendMsg(const UdpMessagePtr& msg) {
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
