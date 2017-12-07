
#include <errno.h>
#include <stddef.h>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

#include <muduo/net/SocketsOps.h>
#include <muduo/base/Logging.h>


#include "TkcpClient.h"





namespace muduo {

namespace net {


TkcpClient::TkcpClient(EventLoop* loop,
                            const InetAddress& peerAddress,
                            const string& nameArg)
    : loop_(CHECK_NOTNULL(loop)),
      name_(nameArg),
      peerAddress_(peerAddress),
      tcpClient_(loop_, peerAddress, nameArg),
      socket_(new UdpClientSocket(loop_, "tkcp")){
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

//    if (conn->connected()) {
//        sess_.reset(new TkcpConnection(0, InetAddress(), conn, name_+":client"));
//        conn->setConnectionCallback(boost::bind(&TkcpConnection::onTcpConnection, sess_, _1));
//        conn->setMessageCallback(boost::bind(&TkcpConnection::onTcpMessage, sess_, _1, _2, _3));
//
//        sess_->SetTkcpConnectionCallback(tkcpConnectionCallback_);
//        sess_->SetTkcpMessageCallback(tkcpMessageCallback_);
//        sess_->SetTkcpCloseCallback(boost::bind(&TkcpClient::removeTckpSession, this, _1));
//        sess_->SetUdpOutCallback(boost::bind(&TkcpClient::outPutUdpMessage, this, _1, _2, _3));
//    }
}







}
}
