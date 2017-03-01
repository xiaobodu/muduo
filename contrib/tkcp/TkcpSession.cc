
#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Buffer.h>
#include <stdint.h>
#include "TkcpSession.h"
#include "Packet.h"
#include "Coding.h"

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

TkcpSession::TkcpSession(uint32_t conv, const InetAddress& localAddressForUdp, const TcpConnectionPtr& tcpConnectionPtr)
    : loop_(tcpConnectionPtr->getLoop()),
      conv_(conv),
      tcpConnectionPtr_(tcpConnectionPtr),
      state_(KConnecting),
      localAddressForUdp_(localAddressForUdp),
      kcpcb_(NULL) {


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
    uint32_t conv = DecodeUint32(msg->buffer().get());
    uint8_t packeId = DecodeUint8(msg->buffer().get());
    switch (packeId) {
        case packet::udp::kData:
            ikcp_input(kcpcb_, msg->buffer()->peek(), msg->buffer()->readableBytes());
            msg->buffer()->retrieveAll();
            break;
        case packet::udp::kConnectSyn:
            LOG_DEBUG << "recv KConnectSyn udpmsg from conv " << conv;

            break;

        default:
            LOG_WARN << "unknown udp message";
    }
}


void TkcpSession::SyncUdpConnectionInfo() {
    Buffer sendbuf(64);
    packet::tcp::UdpConnectionInfo connInfo;
    connInfo.packetId= packet::tcp::kUdpConnectionInfo;
    connInfo.conv = conv_;
    connInfo.ip = localAddressForUdp_.toIp();
    connInfo.port = localAddressForUdp_.toPort();
    connInfo.Encode(&sendbuf);


    tcpConnectionPtr_->send(&sendbuf);
}


void TkcpSession::onTcpConnection(const TcpConnectionPtr& conn) {
    LOG_DEBUG << "tcp conn " << "from " << conn->peerAddress().toIpPort()
        << " " << (conn->connected() ? "UP" : "DOWN");
}


void TkcpSession::onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    while (buf->readableBytes() >= sizeof(uint16_t)) {
        const uint16_t len = static_cast<uint16_t>(buf->peekInt16());

        if (buf->readableBytes() >= implicit_cast<std::size_t>(len)) {
            buf->retrieveInt16();
            int8_t cmd = buf->readInt8();
            switch(cmd) {
                case packet::tcp::kData:
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





