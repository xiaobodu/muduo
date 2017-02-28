
#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <stdint.h>
#include "TkcpSession.h"
#include "TkcpDefine.h"

namespace muduo {

namespace net {


void defaultTkcpConnectionCallback(const TkcpSessionPtr& conn) {
    LOG_TRACE << "(tcp:" << conn->localAddressForTcp().toIpPort()
              << ",udp:" << conn->LocalAddressForUdp().toPort()
              << ")->" << "(tcp:" << conn->peerAddressForTcp().toIpPort()
              << ",udp:" << conn->LocalAddressForUdp().toPort()
              << ") is" << (conn->Connected() ? "UP" : "DOWN");
}

void defaultTkcpMessageCallback(const TkcpSessionPtr&, Buffer* buf, Timestamp) {
    buf->retrieveAll();
}

TkcpSession::TkcpSession(EventLoop* loop, uint32_t conv, InetAddress& localAddressForUdp, TcpConnectionPtr& tcpConnectionPtr)
    : loop_(CHECK_NOTNULL(loop)),
      conv_(conv),
      tcpConnectionPtr_(tcpConnectionPtr),
      state_(KConnecting),
      localAddressForUdp_(localAddressForUdp),
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


// conv +  cmd + data : msg 中 conv 已读出 只剩 cmd + data
void TkcpSession::InUdpMessage(UdpMessagePtr& msg) {
    int8_t cmd = msg->buffer()->readInt8();

    switch (cmd) {
        case kUdpCmdData:
            ikcp_input(kcpcb_, msg->buffer()->peek(), msg->buffer()->readableBytes());
            msg->buffer()->retrieveAll();
            break;
        case kUdpCmdConnectionSyncC2S:
            break;

        default:
            LOG_WARN << "unknown udp message";
    }
}


void TkcpSession::onTcpConnection(const TcpConnectionPtr& conn) {

}


void TkcpSession::onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    while (buf->readableBytes() >= sizeof(uint16_t)) {
        const uint16_t len = static_cast<uint16_t>(buf->peekInt16());

        if (buf->readableBytes() >= implicit_cast<std::size_t>(len)) {
            buf->retrieveInt16();
            int8_t cmd = buf->readInt8();
            switch(cmd) {
                case kTcpCmdData:
                    {
                        std::size_t datalen =  implicit_cast<std::size_t>(len-sizeof(uint16_t)-sizeof(int8_t));
                        Buffer message(datalen);
                        tkcpMessageCallback_(shared_from_this(), &message, receiveTime);
                        break;
                    }
                default:
                    LOG_WARN << "unknown tcp message";
            }
        } else {
            break;
        }
    }
}

}

}





