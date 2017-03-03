
#include <stddef.h>

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

void defaultTkcpMessageCallback(const TkcpSessionPtr&, Buffer* buf) {
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


int TkcpSession::handleKcpOutput(const char* buf, int len) {
    if (udpOutputCallback_) {
        return udpOutputCallback_(shared_from_this(), buf, len);
    }
    return 0;
}

int TkcpSession::KcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user) {
    (void)kcp;
    TkcpSession* sess = static_cast<TkcpSession*>(user);
    return sess->handleKcpOutput(buf, len);
}


void TkcpSession::InputUdpMessage(UdpMessagePtr& msg) {
    peerAddressForUdp_ = msg->intetAddress();
    uint32_t conv = DecodeUint32(msg->buffer().get());
    uint8_t packeId = DecodeUint8(msg->buffer().get());
    LOG_DEBUG << "recv " << packet::udp::PacketIdToString(packeId) <<
                 " udpmsg from conv " << conv;
    switch (packeId) {
        case packet::udp::kData:
            onUdpData(msg->buffer()->peek(), msg->buffer()->readableBytes());
            msg->buffer()->retrieveAll();
            break;
        case packet::udp::kConnectSyn:
            onConnectSyn();
            break;
        case packet::udp::kConnectSynAck:
            onConnectSyncAck();
            break;
        case packet::udp::kConnectAck:
            onConnectAck();
            break;
        case packet::udp::kPingRequest:
            onPingRequest();
            break;
        case packet::udp::kPingReply:
            onPingReply();
            break;
        default:
            LOG_WARN << "unknown udp message";
    }
}

void TkcpSession::onConnectSyn() {
    Buffer sendbuf;
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kConnectSynAck);

    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpSession::onConnectSyncAck() {
    Buffer sendbuf;
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kConnectAck);
    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpSession::onConnectAck() {
}

void TkcpSession::onPingRequest() {
}

void TkcpSession::onPingReply() {
}

void TkcpSession::onUdpData(const char* buf, size_t len) {
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
    while (buf->readableBytes() > packet::tcp::kPacketHeadLength) {

        Buffer headBuf;
        headBuf.append(buf->peek(), packet::tcp::kPacketHeadLength);
        uint16_t len = DecodeUint16(&headBuf);

        if (buf->readableBytes() >= implicit_cast<std::size_t>(len)) {
            uint8_t packeId = DecodeUint8(&headBuf);
            switch(packeId) {
                case packet::tcp::kData:
                    {
                        onTcpData(buf);
                        break;
                    }
                case packet::tcp::kUdpConnectionInfo:
                    {
                        onUdpconnectionInfo(buf);
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

void TkcpSession::onUdpconnectionInfo(Buffer* buf) {
    packet::tcp::UdpConnectionInfo connInfo;
    connInfo.Decode(buf);
    conv_ = connInfo.conv;
    peerAddressForUdp_ = InetAddress(connInfo.ip, connInfo.port);

    Buffer sendbuf(64);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kConnectSyn);
    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}



void TkcpSession::onTcpData(Buffer* buf) {
    uint16_t len = DecodeUint16(buf);
    buf->retrieveInt8();
    size_t dataSize = len - packet::tcp::kPacketHeadLength;
    Buffer message(dataSize);
    message.append(buf->peek(), dataSize);
    buf->retrieve(dataSize);
    tkcpMessageCallback_(shared_from_this(), &message);
}



}
}





