
#include <stddef.h>
#include <assert.h>
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
    loop_->assertInLoopThread();
    LOG_TRACE << "TkcpServer::~TkcpServer [" << name_ << "] destructing";

    for (SessionMap::iterator it(sessions_.begin());
         it != sessions_.end(); ++it) {

        TkcpSessionPtr sess = it->second;
        it->second.reset();
        sess->GetLoop()->runInLoop(boost::bind(&TkcpSession::ConnectDestroyed, sess));
        sess.reset();
    }
}

void TkcpServer::Start() {
    if (started_.getAndSet(1) == 0) {
        tcpserver_.start();
        udpService_.Start();
    }
}

void TkcpServer::newTcpConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        uint32_t conv = nextConv_++;
        TkcpSessionPtr sess = TkcpSessionPtr(new TkcpSession(conv, udpListenAddress_, conn));
        conn->setConnectionCallback(boost::bind(&TkcpSession::onTcpConnection, sess, _1));
        conn->setMessageCallback(boost::bind(&TkcpSession::onTcpMessage, sess, _1, _2, _3));

        sessions_[conv] = sess;
        sess->SetTkcpConnectionCallback(tkcpConnectionCallback_);
        sess->SetTkcpMessageCallback(tkcpMessageCallback_);
        sess->SetTkcpCloseCallback(boost::bind(&TkcpServer::removeTckpSession, this, _1));
        sess->SetUdpOutCallback(boost::bind(&TkcpServer::outPutUdpMessage, this, _1, _2, _3));

        sess->GetLoop()->runInLoop(boost::bind(&TkcpSession::SyncUdpConnectionInfo, sess));
    }

}

int TkcpServer::outPutUdpMessage(const TkcpSessionPtr& sess, const char* buf, size_t len) {
    UdpMessagePtr message(new UdpMessage(buf, len, sess->peerAddressForUdp()));
    udpService_.SendMsg(message);
    return 0;
}

void TkcpServer::removeTckpSession(const TkcpSessionPtr& sess) {
    loop_->runInLoop(boost::bind(&TkcpServer::removeTckpSessionInLoop, this, sess));
}

void TkcpServer::removeTckpSessionInLoop(const TkcpSessionPtr& sess) {
    loop_->assertInLoopThread();

    size_t n = sessions_.erase(sess->conv());
    (void)n;
    assert(n == 1);
    EventLoop* ioLoop = sess->GetLoop();
    ioLoop->queueInLoop(boost::bind(&TkcpSession::ConnectDestroyed, sess));
}

void TkcpServer::onUdpMessage(UdpMessagePtr& msg) {
    uint32_t conv = static_cast<uint32_t>(msg->buffer()->peekInt32());

    SessionMap::const_iterator iter = sessions_.find(conv);

    assert(iter != sessions_.end());
    if (iter != sessions_.end()) {
        iter->second->InputUdpMessage(msg);
    }
}

}
}
