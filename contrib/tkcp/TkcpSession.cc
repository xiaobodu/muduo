
#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include "TkcpSession.h"
#include "TkcpDefine.h"

namespace muduo {

namespace net {

TkcpSession::TkcpSession(EventLoop* loop, uint32_t conv, TcpConnectionPtr& tcpConnectionPtr)
    : loop_(CHECK_NOTNULL(loop)),
      conv_(conv),
      tcpConnectionPtr_(tcpConnectionPtr),
      state_(KConnecting),
      peerAddrForUdp_(),
      kcpcb_(NULL) {

      tcpConnectionPtr_->setConnectionCallback(boost::bind(&TkcpSession::onTcpConnection, shared_from_this(), _1));
      tcpConnectionPtr_->setMessageCallback(boost::bind(&TkcpSession::onTcpMessage, shared_from_this(), _1, _2, _3));

}

TkcpSession::~TkcpSession() {
    if (kcpcb_ != NULL) {
        ikcp_release(kcpcb_);
        kcpcb_ = NULL;
    }
}


int TkcpSession::handleKcpout(const char* buf, int len) {
    if (udpOutputCallback_) {
        return udpOutputCallback_(shared_from_this(), buf, len);
    }
    return 0;
}

int TkcpSession::KcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user) {
    (void)kcp;
    TkcpSession* sess = static_cast<TkcpSession*>(user);
    return sess->handleKcpout(buf, len);
}


void TkcpSession::InUdpMessage(UdpMessagePtr& msg) {
    int8_t cmd = msg->buffer()->readInt8();

    switch (cmd) {
        case kUdpCmdData:
            ikcp_input(kcpcb_, msg->buffer()->peek(), msg->buffer()->readableBytes());
            msg->buffer()->retrieveAll();
            break;
        default:
            LOG_WARN << "unknow udp message";
    }
}


void TkcpSession::onTcpConnection(const TcpConnectionPtr& conn) {
}

void TkcpSession::onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
}

}

}





