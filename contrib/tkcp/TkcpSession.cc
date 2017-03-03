
#include <stddef.h>
#include <sys/time.h>

#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <stdint.h>
#include "TkcpSession.h"
#include "Packet.h"
#include "Coding.h"

namespace muduo {

namespace net {



const int KMicroSecondsPerMillissecond = 1000;
const int kMillissecondsPerSecond = 1000;
static double MillisecondToSecond(int millisecond) {
    return static_cast<double>(millisecond) / kMillissecondsPerSecond;
}

static uint32_t Clock() {
    struct timeval tv;
    ::gettimeofday(&tv, NULL);
    uint64_t millisecond = static_cast<uint64_t>(tv.tv_sec) * kMillissecondsPerSecond+
                           (tv.tv_usec / KMicroSecondsPerMillissecond);

    return static_cast<uint32_t>(millisecond & 0xfffffffful);
}


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
    assert(kcpcb_ == NULL);
}

void TkcpSession::Send(const void* data, int len) {
    Send(StringPiece(static_cast<const char*>(data), len));
}

void TkcpSession::Send(const StringPiece& message) {
    if (state_ == KConnected) {
        if (loop_->isInLoopThread()) {
            SendInLoop(message);
        } else {
            loop_->runInLoop(boost::bind(&TkcpSession::SendInLoop,
                                         this,
                                         message.as_string()));
        }
    }
}

void TkcpSession::Send(Buffer* buf) {
    if (state_ == KConnected) {
        if (loop_->isInLoopThread()) {
            SendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        } else {
            loop_->runInLoop(boost::bind(&TkcpSession::SendInLoop,
                                         this,
                                         buf->retrieveAllAsString()));
        }
    }
}

void TkcpSession::SendInLoop(const StringPiece& message) {
    SendInLoop(message.data(), message.size());
}

void TkcpSession::SendInLoop(const void *data, size_t len) {
    loop_->assertInLoopThread();
    if (state_ == KDisconnected) {
        LOG_WARN << "Disconnected, give up writing";
        return;
    }

    SendKcpMsg(data, len);
}

void TkcpSession::SendKcpMsg(const void *data, size_t len) {

    ikcp_send(kcpcb_, static_cast<const char*>(data), static_cast<int>(len));
}


int TkcpSession::handleKcpOutput(const char* buf, int len) {
    if (udpOutputCallback_) {
        return udpOutputCallback_(shared_from_this(), buf, len);
    }
    return 0;
}

int TkcpSession::kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user) {
    (void)kcp;
    TkcpSession* sess = static_cast<TkcpSession*>(user);
    return sess->handleKcpOutput(buf, len);
}

// safe
void TkcpSession::InputUdpMessage(UdpMessagePtr& msg) {
    loop_->assertInLoopThread();
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
    initKcp(); // client
}

void TkcpSession::onConnectAck() {
    initKcp(); // server
}

void TkcpSession::kcpUpdate() {
    uint32_t now =  Clock();
    ikcp_update(kcpcb_, now);
    uint32_t  nextTime = ikcp_check(kcpcb_, now);

    loop_->runAfter(MillisecondToSecond(nextTime),
                    boost::bind(&TkcpSession::kcpUpdate, this));
}


void TkcpSession::initKcp() {
    assert(kcpcb_ == NULL);
    kcpcb_  = ikcp_create(conv_, this);
    ikcp_setoutput(kcpcb_, kcpOutput);
    ikcp_nodelay(kcpcb_, 1, 10, 2, 1);
    ikcp_wndsize(kcpcb_, 128, 128);
    ikcp_setmtu(kcpcb_, 576 - 64 - packet::udp::kPacketHeadLength);
    kcpcb_->rx_minrto = 10;

    loop_->runAfter(MillisecondToSecond(kcpcb_->interval),
                    boost::bind(&TkcpSession::kcpUpdate, this));

    setState(KConnected);
    tkcpConnectionCallback_(shared_from_this());
}

void TkcpSession::onPingRequest() {
}

void TkcpSession::onPingReply() {
}

void TkcpSession::onUdpData(const char* buf, size_t len) {
    ikcp_input(kcpcb_, buf, static_cast<int>(len));

    int size = ikcp_peeksize(kcpcb_);
    if (size > 0) {
        Buffer message(static_cast<size_t>(size));
        message.append(buf, len);
        tkcpMessageCallback_(shared_from_this(), &message);
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
    loop_->assertInLoopThread();
    LOG_DEBUG << "tcp conn " << "from " << conn->peerAddress().toIpPort()
        << " " << (conn->connected() ? "UP" : "DOWN");
}


void TkcpSession::onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    loop_->assertInLoopThread();
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

void TkcpSession::ConnectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == KConnected) {
        setState(KDisconnected);
        tkcpConnectionCallback_(shared_from_this());
    }
    loop_->cancel(kcpUpdateTimer);
    ikcp_release(kcpcb_);
    kcpcb_ = NULL;
}


}
}





