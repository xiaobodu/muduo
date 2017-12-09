
#include <errno.h>
#include <stddef.h>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

#include <muduo/net/SocketsOps.h>
#include <muduo/base/Logging.h>
#include <memory>


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
    socket_->SetConnectCallback(boost::bind(&TkcpClient::onUdpConnect, this, _1, _2));
    socket_->SetDisconnectCallback(boost::bind(&TkcpClient::onUdpDisconnect, this, _1, _2));
    socket_->SetMessageCallback(boost::bind(&TkcpClient::onUdpMessage, this, _1, _2, _3));
}

TkcpClient::~TkcpClient() {
}

void TkcpClient::Connect() {
    socket_->Connect(peerAddress_);
}

void TkcpClient::Disconnect() {
    tcpClient_.disconnect();
}

void TkcpClient::onUdpConnect(const UdpClientSocketPtr&, UdpClientSocket::ConnectResult result) {
    if (result == UdpClientSocket::ConnectResult::kConnectSuccess) {
        tcpClient_.connect();
    } else if (result == UdpClientSocket::ConnectResult::kConnectFailure) {
        LOG_FATAL << "udp socket connect failed";
    } else {
        LOG_WARN << "udp socket already connect";
    }
}

void TkcpClient::onUdpDisconnect(const UdpClientSocketPtr&, UdpClientSocket::DisconnectResutl result) {
    LOG_INFO << "udp disconnect " << (result == UdpClientSocket::DisconnectResutl::kDisconnectSuccess ?  " ssuccess " : " failed");
}

void TkcpClient::onUdpMessage(const UdpClientSocketPtr&, Buffer* buf, Timestamp) {
    conn_->InputUdpMessage(buf, peerAddress_);
}

void TkcpClient::newTcpConnection(const TcpConnectionPtr& conn) {

    if (conn->connected()) {
        InetAddress localUdpAddress;
        socket_->LocalAddress(&localUdpAddress);
        conn_.reset(new TkcpConnection(0, localUdpAddress, peerAddress_, conn, name_ + ":client"));
        conn->setConnectionCallback(boost::bind(&TkcpConnection::onTcpConnection, conn_, _1));
        conn->setMessageCallback(boost::bind(&TkcpConnection::onTcpMessage, conn_, _1, _2, _3));

        conn_->SetTkcpConnectionCallback(connectionCallback_);
        conn_->SetTkcpMessageCallback(messageCallback_);
        conn_->SetTkcpCloseCallback(boost::bind(&TkcpClient::removeTckpconnection, this, _1));
        conn_->SetUdpOutCallback(boost::bind(&TkcpClient::outputUdpMessage, this, _1, _2, _3));
    }
}

int TkcpClient::outputUdpMessage(const TkcpConnectionPtr&, const char* buf, size_t len) {
    socket_->Write(buf, len);
    return 0;
}

void TkcpClient::removeTckpconnection(const TkcpConnectionPtr&) {
    conn_.reset();
    socket_->Disconnect();
    LOG_INFO << "TckpClient::removeTckpconnection";
}
}
}
