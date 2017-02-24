#include <errno.h>

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include <muduo/net/SocketsOps.h>
#include <muduo/base/Logging.h>
#include "UdpService.h"




using namespace muduo;
using namespace muduo::net;





UdpService::UdpService(const InetAddress& localaddr)
    : udpSocket_(localaddr.family() == PF_INET ? UdpSocket::IPV4 : UdpSocket::IPV6),
      started_(false) {

    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetReuseAddr(true);
    udpSocket_.SetTosWithLowDelay();
    udpSocket_.BindAddress(localaddr);
}


UdpService::~UdpService() {
}

void UdpService::Start() {
    if (started_) {
        return;
    }

    started_ = true;

    recvLoopThread_.reset(new EventLoopThread(boost::bind(&UdpService::recvLoopThreadInitCallback, this, _1)));
    recvLoop_ = recvLoopThread_->startLoop();

    sendLoopThread_.reset(new EventLoopThread(boost::bind(&UdpService::sendLoopThreadInitCallback, this, _1)));
    sendLoop_ = sendLoopThread_->startLoop();

    LOG_TRACE << "UdpService::Start";
}

void UdpService::Stop() {
}

void UdpService::stopInRecvLoop() {
    if (!started_) {
        return;
    }

    started_ = false;

    LOG_TRACE << "UdpService::stopInLoop";
}

void UdpService::recvLoopThreadInitCallback(EventLoop* loop) {
    channel_.reset(new Channel(loop, udpSocket_.sockfd()));
    channel_->setReadCallback(boost::bind(&UdpService::handleRead, this, _1));
    channel_->setWriteCallback(boost::bind(&UdpService::handleWrite, this));
    channel_->setErrorCallback(boost::bind(&UdpService::handleError, this));
    channel_->tie(shared_from_this());
    channel_->enableReading();
}

void UdpService::sendLoopThreadInitCallback(EventLoop* loop) {
    (void)loop;
}

// runing in recvThread;
void UdpService::handleRead(Timestamp receiveTime) {
    LOG_TRACE << "UdpService " << receiveTime.toFormattedString();
    recvLoop_->assertInLoopThread();

    const std::size_t initialSize = 1472;

    for (;;) {

        struct sockaddr fromAddr;
        ::bzero(&fromAddr, sizeof fromAddr);
        socklen_t addrLen = sizeof fromAddr;
        boost::shared_ptr<Buffer> recvBuffer = boost::make_shared<Buffer>(initialSize);

        int savedError = 0;
        ssize_t nr = ::recvfrom(udpSocket_.sockfd(), recvBuffer->beginWrite(),
                recvBuffer->writableBytes(), 0, &fromAddr, &addrLen);
        savedError = errno;

        if (nr >= 0) {
            recvBuffer->hasWritten(nr);
            InetAddress intetAddress;
            intetAddress.setSockAddrInet6(*sockets::sockaddr_in6_cast(&fromAddr));
            udpMessageCallback_(shared_from_this(),
                                boost::make_shared<UdpMessage>(recvBuffer, intetAddress),
                                receiveTime);
        } else {

            if (savedError == EWOULDBLOCK || savedError == EAGAIN) {
                return;
            } else {
                errno = savedError;
                LOG_SYSERR << "UdpService::handleRead";
                handleError();
                return;
            }
        }
    }
}


void UdpService::handleWrite() {
    LOG_ERROR << "UdpService::handleWrite NOT implement";
}

void UdpService::handleError() {
    int err = sockets::getSocketError(channel_->fd());
    LOG_ERROR << "UdpService::handleError [" << name_
        << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}



void UdpService::SendMsg(UdpMessagePtr& msg) {

    if (sendLoop_->isInLoopThread()) {
        SendMsgInSendLoop(msg);
    } else {
        sendLoop_->runInLoop(boost::bind(&UdpService::SendMsgInSendLoop, this, msg));
    }
}

void UdpService::SendMsgInSendLoop(UdpMessagePtr& msg) {
    ssize_t nw = ::sendto(udpSocket_.sockfd(),
                          msg->buffer()->peek(),
                          msg->buffer()->readableBytes(),
                          0,
                          msg->intetAddress().getSockAddr(),
                          sizeof(struct sockaddr_in6));
    if (nw >= 0) {
        msg->buffer()->retrieve(nw);
    } else {
    }
}
