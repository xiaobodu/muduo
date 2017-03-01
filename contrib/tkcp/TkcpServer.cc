
#include <boost/bind.hpp>

#include <muduo/base/Logging.h>

#include "TkcpServer.h"




namespace muduo {

namespace net {

TkcpServer::TkcpServer(EventLoop* loop,
                       const InetAddress& tcpListenAddr,
                       const InetAddress& udpListenAddr,
                       const string& nameArg)
    : loop_(CHECK_NOTNULL(loop)),
      tcpListenAddress_(tcpListenAddr),
      udpListenAddress_(udpListenAddr),
      name_(nameArg),
      tkcpConnectionCallback_(defaultTkcpConnectionCallback),
      tkcpMessageCallback_(defaultTkcpMessageCallback),
      nextConv_(1),
      udpService_(loop, udpListenAddr),
      tcpserver_(loop, tcpListenAddr, nameArg+":Tkcp") {

    tcpserver_.setConnectionCallback(boost::bind(&TkcpServer::newTcpConnection, this, _1));
    udpService_.SetUdpMessageCallback(boost::bind(&TkcpServer::onUdpMessage, this, _1));

}

TkcpServer::~TkcpServer() {
}
void TkcpServer::Start() {
    if (started_.getAndSet(1) == 0) {
        tcpserver_.start();
        udpService_.Start();
    }
}

void TkcpServer::newTcpConnection(const TcpConnectionPtr& conn) {
    LOG_DEBUG << "tcp conn " << "from " << conn->peerAddress().toIpPort()
              << " " << (conn->connected() ? "UP" : "DOWN");
    if (conn->connected()) {
        uint32_t conv = nextConv_++;
        TkcpSessionPtr sess = TkcpSessionPtr(new TkcpSession(conv, udpListenAddress_, conn));
        conn->setConnectionCallback(boost::bind(&TkcpSession::onTcpConnection, sess, _1));
        conn->setMessageCallback(boost::bind(&TkcpSession::onTcpMessage, sess, _1, _2, _3));

        sessions_[conv] = sess;
        sess->SetTkcpConnectionCallback(tkcpConnectionCallback_);
        sess->SetTkcpMessageCallback(tkcpMessageCallback_);
        sess->SetTkcpCloseCallback(boost::bind(&TkcpServer::removeTckpSession, this, _1));

        sess->GetLoop()->runInLoop(boost::bind(&TkcpSession::SyncUdpConnectionInfo, sess));
    }



}

void TkcpServer::removeTckpSession(const TkcpSessionPtr& sess) {

}

void TkcpServer::removeTckpSessionInLoop(const TkcpSessionPtr& sess) {

}

void TkcpServer::onUdpMessage(UdpMessagePtr& msg) {
    LOG_DEBUG << "new udp message from " << msg->intetAddress().toIpPort();
    uint32_t conv = static_cast<uint32_t>(msg->buffer()->peekInt32());

    SessionMap::const_iterator iter = sessions_.find(conv);

    if (iter != sessions_.end()) {
        iter->second->InUdpMessage(msg);
    }
}

}
}
