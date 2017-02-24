
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include "UdpService.h"




using namespace muduo;
using namespace muduo::net;





UdpService::UdpService(EventLoop* loop, const InetAddress& localaddr)
    : loop_(CHECK_NOTNULL(loop)),
      udpMsgRecvThread_(new muduo::Thread(boost::bind(&UdpService::runInUdpMsgRecvThread, this))),
      udpMsgSendThread_(new muduo::Thread(boost::bind(&UdpService::runInUdpMsgSendThread, this))),
      udpSocket_(localaddr.family() == PF_INET ? UdpSocket::IPV4 : UdpSocket::IPV6),
      running_(false) {

    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetTosWithLowDelay();
    udpSocket_.SetRecvTimeout(500);

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
             loop_->runInLoop(boost::bind(udpMessageCallback_, message));
         }
    }
    LOG_TRACE << "UdpMsgRecvThread Stop";
}

void UdpService::runInUdpMsgSendThread() {
    while(running_) {
        udpSocket_.SendMsg(udpMsgSendQueue_.take());
    }

    LOG_TRACE << "UdpMsgSendThread stop";
}

void UdpService::SendMsg(UdpMessagePtr& msg) {
    udpMsgSendQueue_.put(msg);
}
