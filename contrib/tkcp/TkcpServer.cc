
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <boost/bind.hpp>

#include <muduo/base/Logging.h>

#include "TkcpServer.h"
#include <muduo/net/udp/UdpServerSocket.h>




namespace muduo {

namespace net {

TkcpServer::TkcpServer(EventLoop* loop,
                       const InetAddress& listenAddress,
                       const string& nameArg,
                       const int redundant)
    : loop_(CHECK_NOTNULL(loop)),
      listenAddress_(listenAddress),
      name_(nameArg),
      redundant_(redundant),
      tkcpConnectionCallback_(defaultTkcpConnectionCallback),
      tkcpMessageCallback_(defaultTkcpMessageCallback),
      nextConv_(1),
      socket_(new UdpServerSocket(loop_, listenAddress_, "Tkcp")),
      tcpserver_(loop, listenAddress_, "Tkcp") {

    tcpserver_.setConnectionCallback(boost::bind(&TkcpServer::newTcpConnection, this, _1));
    socket_->SetMessageCallback(boost::bind(&TkcpServer::onUdpMessage, this, _1, _2, _3, _4));
}

TkcpServer::~TkcpServer() {
    loop_->assertInLoopThread();
    LOG_TRACE << "TkcpServer::~TkcpServer [" << name_ << "] destructing";

    for (ConnectionMap::iterator it(connections_.begin());
         it != connections_.end(); ++it) {

        TkcpConnectionPtr sess = it->second;
        it->second.reset();
        sess->getLoop()->runInLoop(boost::bind(&TkcpConnection::ConnectDestroyed, sess));
        sess.reset();
    }
}

void TkcpServer::Start() {
    if (started_.getAndSet(1) == 0) {
        tcpserver_.start();
        socket_->Start();
    }
}

void TkcpServer::newTcpConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        uint32_t conv = nextConv_++;

        char buf[64];
        snprintf(buf, sizeof(buf), "-%s-%d:%d", conn->name().c_str(), listenAddress_.toPort(),  conv);
        string sessName = name_ + buf;

        LOG_INFO << "TkcpServer::newTkcpConnection [" << name_
                 << "] - new sess [" << sessName
                 << "]";
        conn->setTcpNoDelay(true);
        TkcpConnectionPtr sess = TkcpConnectionPtr(new TkcpConnection(conv, listenAddress_, InetAddress(), conn, sessName, redundant_));
        conn->setConnectionCallback(boost::bind(&TkcpConnection::onTcpConnection, sess, _1));
        conn->setMessageCallback(boost::bind(&TkcpConnection::onTcpMessage, sess, _1, _2, _3));

        connections_[conv] = sess;
        sess->SetTkcpConnectionCallback(tkcpConnectionCallback_);
        sess->SetTkcpMessageCallback(tkcpMessageCallback_);
        sess->SetTkcpCloseCallback(boost::bind(&TkcpServer::removeTckpSession, this, _1));
        sess->SetUdpOutCallback(boost::bind(&TkcpServer::outPutUdpMessage, this, _1, _2, _3));

        sess->getLoop()->runInLoop(boost::bind(&TkcpConnection::SyncUdpConnectionInfo, sess));
    }

}

int TkcpServer::outPutUdpMessage(const TkcpConnectionPtr& sess, const char* buf, size_t len) {
    socket_->SendTo(buf, len, sess->peerUdpAddress());
    return 0;
}

void TkcpServer::removeTckpSession(const TkcpConnectionPtr& sess) {
    loop_->runInLoop(boost::bind(&TkcpServer::removeTckpSessionInLoop, this, sess));
}

void TkcpServer::removeTckpSessionInLoop(const TkcpConnectionPtr& sess) {
    loop_->assertInLoopThread();

    size_t n = connections_.erase(sess->conv());
    (void)n;
    assert(n == 1);
    LOG_DEBUG << sess->name() << " count " << sess.use_count();
    EventLoop* ioLoop = sess->getLoop();
    ioLoop->runInLoop(boost::bind(&TkcpConnection::ConnectDestroyed, sess));
}

void TkcpServer::onUdpMessage(const UdpServerSocketPtr& socket, Buffer* buf, Timestamp time,
                                     const InetAddress& peerAddress){
    uint32_t conv = static_cast<uint32_t>(buf->peekInt32());

    ConnectionMap::const_iterator iter = connections_.find(conv);


    if (iter != connections_.end()) {
        iter->second->InputUdpMessage(buf, peerAddress);
    }
}

}
}
