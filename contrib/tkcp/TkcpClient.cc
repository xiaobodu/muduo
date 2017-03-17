
#include <errno.h>
#include <stddef.h>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

#include <muduo/net/SocketsOps.h>
#include <muduo/base/Logging.h>


#include "TkcpClient.h"
#include "UdpMessage.h"
#include "UdpSocket.h"




namespace muduo {

namespace net {


TkcpClient::TkcpClient(EventLoop* loop,
                            const InetAddress& serverAddrForTcp,
                            const string& nameArg)
    : loop_(CHECK_NOTNULL(loop)),
      name_(nameArg),
      serverAddrForTcp_(serverAddrForTcp),
      udpsocketConnected(false),
      tcpClient_(loop_, serverAddrForTcp, nameArg) {
    tcpClient_.setConnectionCallback(boost::bind(&TkcpClient::newTcpConnection, this, _1));
}

TkcpClient::~TkcpClient() {
}

void TkcpClient::Connect() {
    tcpClient_.connect();
}

void TkcpClient::Disconnect() {
    tcpClient_.disconnect();
}

void TkcpClient::newTcpConnection(const TcpConnectionPtr& conn) {

    if (conn->connected()) {
        sess_.reset(new TkcpSession(0, InetAddress(), conn, name_+":client"));
        conn->setConnectionCallback(boost::bind(&TkcpSession::onTcpConnection, sess_, _1));
        conn->setMessageCallback(boost::bind(&TkcpSession::onTcpMessage, sess_, _1, _2, _3));

        sess_->SetTkcpConnectionCallback(tkcpConnectionCallback_);
        sess_->SetTkcpMessageCallback(tkcpMessageCallback_);
        sess_->SetTkcpCloseCallback(boost::bind(&TkcpClient::removeTckpSession, this, _1));
        sess_->SetUdpOutCallback(boost::bind(&TkcpClient::outPutUdpMessage, this, _1, _2, _3));
    }
}

void TkcpClient::removeTckpSession(const TkcpSessionPtr& sess) {
    LOG_INFO << "TckpClient::removeTckpSession";
}


void TkcpClient::handleRead(Timestamp receiveTime) {
    loop_->assertInLoopThread();

    const size_t initialSize = 1472;
    boost::shared_ptr<Buffer> recvBuffer = boost::make_shared<Buffer>(initialSize);
    ssize_t nr = ::recv(udpChannel_->fd(),
                        recvBuffer->beginWrite(),
                        recvBuffer->writableBytes(),
                        0);

    if (nr >= 0) {
        recvBuffer->hasWritten(nr);
        UdpMessagePtr message = boost::make_shared<UdpMessage>(recvBuffer, InetAddress());
        sess_->InputUdpMessage(message);
        LOG_DEBUG << "recv return, readn = " << nr;
    } else {
        LOG_SYSERR << "TkcpClient::handleRead";
        handError();
    }

}

void TkcpClient::handWrite() {
    loop_->assertInLoopThread();

    if (udpChannel_->isWriting()) {
       if (outputUdMessagesCache_.empty()) {
           udpChannel_->disableAll();
           return;
       }
       string& msg = outputUdMessagesCache_.front();
       int ret = sendUdpMsg(msg.c_str(), msg.size());
       if (ret >= 0) {
           outputUdMessagesCache_.pop();
       }
    }
}

int TkcpClient::sendUdpMsg(const char* buf, size_t len) {
    ssize_t nw = ::send(udpChannel_->fd(), buf, len, 0);

    if (nw >= 0) {
        return 0;
    } else {
        if (errno != EWOULDBLOCK || errno != EAGAIN || errno != EINTR) {
            LOG_SYSERR << "TkcpClient::sendUdpMsg";
        }
        return -1;
    }
}


void TkcpClient::handError() {
    loop_->assertInLoopThread();
    int err = sockets::getSocketError(udpChannel_->fd());

    LOG_ERROR << "TkcpClient::handError [" << name_ <<
                 "] - SO_ERROR = " << err << " " << strerror_tl(err);

}

void TkcpClient::connectUdpsocket() {
    udpsocketConnected = true;
    udpsocket_.reset(new UdpSocket(UdpSocket::IPV4, true));
    udpsocket_->ConnectAddress(serverAddrForUdp_);
    udpChannel_.reset(new Channel(loop_,udpsocket_->sockfd()));
    udpChannel_->setReadCallback(boost::bind(&TkcpClient::handleRead, this, _1));
    udpChannel_->setWriteCallback(boost::bind(&TkcpClient::handWrite, this));
    udpChannel_->setErrorCallback(boost::bind(&TkcpClient::handError, this));
    udpChannel_->enableReading();
}

//in loop
int TkcpClient::outPutUdpMessage(const TkcpSessionPtr& sess, const char* buf, size_t len) {

    if (!udpsocketConnected) {
        serverAddrForUdp_ = sess->peerAddressForUdp();
        connectUdpsocket();
    }

    if (udpChannel_->isWriting()) {
        outputUdMessagesCache_.push(string(buf,len));
        return 0;
    }

    int ret = sendUdpMsg(buf, len);
    if (ret < 0) {
        outputUdMessagesCache_.push(string(buf,len));
        udpChannel_->enableWriting();
        return 0;
    }

    return 0;
}

}
}
