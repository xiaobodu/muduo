
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
#include "UdpMessage.h"
#include "Fec.h"

namespace muduo {

namespace net {



const int kMicroSecondsPerMillissecond = 1000;
const int kMillissecondsPerSecond = 1000;
static double MillisecondToSecond(int millisecond) {
    return static_cast<double>(millisecond) / kMillissecondsPerSecond;
}


static uint32_t TimestampToMillisecond(const Timestamp time) {
    uint64_t millisecond = static_cast<uint64_t>(time.microSecondsSinceEpoch()/kMicroSecondsPerMillissecond);
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

TkcpSession::TkcpSession(uint32_t conv,
        const InetAddress& localAddressForUdp,
        const TcpConnectionPtr& tcpConnectionPtr,
        const string& nameArg)
    :
      loop_(tcpConnectionPtr->getLoop()),
      conv_(conv),
      name_(nameArg),
      tcpConnectionPtr_(tcpConnectionPtr),
      state_(kTcpConnected),
      localAddressForUdp_(localAddressForUdp),
      kcpInited_(false),
      kcpcb_(NULL),
      kcpRecvBuf_(2048),
      trySendConnectSynTimes(0),
      udpAvailble_(true),
      nextKcpUpdateTime_(0) {

    fec_.reset(new Fec());
    fec_->setSendOutCallback(boost::bind(&TkcpSession::onFecSendData, this, _1, _2));
    fec_->setRecvOutCallback(boost::bind(&TkcpSession::onFecRecvData, this, _1, _2));

    LOG_DEBUG << "TkcpSession::ctor[" << name_ << "] at" << this;
}

TkcpSession::~TkcpSession() {
    assert(kcpcb_ == NULL);

    LOG_DEBUG << "TkcpSession::dtor[" << name_ << "] at" << this
              << " state=" << stateToString();
    assert(kcpcb_ == NULL);
    assert(state_ == kDisconnected);
}

const char* TkcpSession::stateToString() const {
    switch(state_) {
        case kTcpConnected:
            return "kTcpConnected";
        case kUdpConnectSynSend:
            return "kUdpConnectSynSend";
        case kConnected:
            return "KConnected";
        case kDisconnecting:
            return "kDisconnecting";
        case kDisconnected:
            return "kDisconnected";
        default:
            return "unknown state";
    }
}

void TkcpSession::Send(const void* data, int len) {
    Send(StringPiece(static_cast<const char*>(data), len));
}

void TkcpSession::Send(const StringPiece& message) {
    if (state_ == kConnected) {
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
    if (state_ == kConnected) {
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
    if (state_ != kConnected) {
        LOG_WARN << "Disconnected, give up writing";
        return;
    }

    if (udpAvailble_) {
        sendKcpMsg(data, len);
    } else {
        sendTcpMsg(data, len);
    }

}

void TkcpSession::sendKcpMsg(const void *data, size_t len) {
    ikcp_send(kcpcb_, static_cast<const char*>(data), static_cast<int>(len));
    ikcp_flush(kcpcb_);
}

void TkcpSession::sendTcpMsg(const void *data, size_t len) {
    Buffer sendBuf;
    EncodeUint16(&sendBuf, static_cast<uint16_t>(len + packet::tcp::kPacketHeadLength));
    EncodeUint8(&sendBuf, packet::tcp::kData);
    sendBuf.append(data, len);
    tcpConnectionPtr_->send(&sendBuf);

}


int TkcpSession::handleKcpOutput(const char* buf, int len) {
    fec_->Send(buf, static_cast<size_t>(len));
    return 0;
}

void TkcpSession::onFecSendData(const char* data, size_t len) {

    if (udpOutputCallback_) {
        EncodeUint32(&udpSendBuf_, conv_);
        EncodeUint8(&udpSendBuf_, packet::udp::kData);
        udpSendBuf_.append(data, len);
        udpOutputCallback_(shared_from_this(), udpSendBuf_.peek(), udpSendBuf_.readableBytes());
        udpSendBuf_.retrieveAll();
    }
}

int TkcpSession::kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user) {
    (void)kcp;
    TkcpSession* sess = static_cast<TkcpSession*>(user);
    return sess->handleKcpOutput(buf, len);
}

// safe
void TkcpSession::InputUdpMessage(UdpMessagePtr& msg) {
    loop_->assertInLoopThread();
    assert(state_ == kTcpConnected ||
           state_ == kUdpConnectSynSend ||
           state_ == kConnected);
    if (state_ != kTcpConnected &&
        state_ != kUdpConnectSynSend &&
        state_ != kConnected) {
        return;
    }

    if (!udpAvailble_) {
        return;
    }

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
    if (!kcpInited_) {
        initKcp();
    }
    Buffer sendbuf(8);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kConnectSynAck);

    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpSession::onConnectSyncAck() {
    if (!kcpInited_) {
        assert(state_ == kUdpConnectSynSend);
        initKcp(); // client
        udpPingTimer_ = loop_->runEvery(60, boost::bind(&TkcpSession::udpPingRequest, this));
        tcpPingTimer_ = loop_->runAfter(120, boost::bind(&TkcpSession::tcpPingRequest, this));
    }
}

void TkcpSession::kcpUpdate() {
    uint32_t now =  TimestampToMillisecond(loop_->pollReturnTime());
    if (now > nextKcpUpdateTime_) {
        ikcp_update(kcpcb_, now);
        nextKcpUpdateTime_ = ikcp_check(kcpcb_, now);
    }
}


void TkcpSession::initKcp() {
    assert(kcpcb_ == NULL);
    assert(kcpInited_ == false);
    assert(state_ == kTcpConnected || state_ == kUdpConnectSynSend);
    kcpInited_ = true;
    kcpcb_  = ikcp_create(conv_, this);
    ikcp_setoutput(kcpcb_, kcpOutput);
    ikcp_nodelay(kcpcb_, 1, 30, 2, 1);
    ikcp_wndsize(kcpcb_, 128, 128);
    ikcp_setmtu(kcpcb_, (576 - 20 - 8 - packet::udp::kPacketHeadLength - 3 * Fec::FecHeadLen)/3);
    kcpcb_->rx_minrto = 10;

    kcpUpdateTimer_ = loop_->runEvery(MillisecondToSecond(30),
                    boost::bind(&TkcpSession::kcpUpdate, this));

    setState(kConnected);
    tkcpConnectionCallback_(shared_from_this());
}

void TkcpSession::onPingRequest() {
    lastRecvUdpPingDataTime_ = loop_->pollReturnTime();
    Buffer sendbuf(8);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kPingReply);
    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpSession::onPingReply() {
    lastRecvUdpPingDataTime_ = loop_->pollReturnTime();
}

void TkcpSession::udpPingRequest() {
    Buffer sendbuf(8);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kPingRequest);
    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpSession::onUdpData(const char* buf, size_t len) {

    if (state_ != kConnected) {
        return;
    }

    fec_->Input(buf, len);
}

void TkcpSession::onFecRecvData(const char* data, size_t len) {
    ikcp_input(kcpcb_, data, static_cast<int>(len));
    int size = ikcp_peeksize(kcpcb_);
    if (size > 0) {

        int nr = ikcp_recv(kcpcb_, kcpRecvBuf_.beginWrite(), static_cast<int>(kcpRecvBuf_.writableBytes()));
        assert(size == nr);
        kcpRecvBuf_.hasWritten(nr);
        tkcpMessageCallback_(shared_from_this(), &kcpRecvBuf_);
        kcpRecvBuf_.retrieveAll();
    }
    ikcp_flush(kcpcb_);
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
    assert(conn->disconnected());
    if (conn->disconnected()) {

        assert(state_ == kConnected || state_ == kTcpConnected || state_ == kUdpConnectSynSend);
        setState(kDisconnected);

        loop_->cancel(udpPingTimer_);
        loop_->cancel(tcpPingTimer_);
        loop_->cancel(kcpUpdateTimer_);

        TkcpSessionPtr guardThis(shared_from_this());
        tkcpConnectionCallback_(guardThis);


        tkcpCloseCallback_(guardThis);
    }

}


void TkcpSession::onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    loop_->assertInLoopThread();
    while (buf->readableBytes() >= packet::tcp::kPacketHeadLength) {
        Buffer headBuf;
        headBuf.append(buf->peek(), packet::tcp::kPacketHeadLength);
        uint16_t len = DecodeUint16(&headBuf);

        if (buf->readableBytes() >= implicit_cast<std::size_t>(len)) {
            uint8_t packeId = DecodeUint8(&headBuf);
            LOG_DEBUG << "recv tcp message " << packet::tcp::PacketIdToString(packeId)
                      << "from " << " conv " << conv_;
            switch(packeId) {
                case packet::tcp::kData:
                    onTcpData(buf);
                    break;
                case packet::tcp::KUseTcp:
                    onUseTcp(buf);
                    break;
                case packet::tcp::kUdpConnectionInfo:
                    onUdpconnectionInfo(buf);
                    break;
                case packet::tcp::kPingRequest:
                    onTcpPingRequest(buf);
                    break;
                case packet::tcp::kPingReply:
                    onTcpPingReply(buf);
                    break;
                default:
                    buf->retrieve(len);
                    LOG_WARN << "unknown tcp message " << packeId << " from " << tcpConnectionPtr_->peerAddress().toIpPort();
                    forceClose();
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

    setState(kUdpConnectSynSend);
    sendConnectSyn();
}
void TkcpSession::sendConnectSyn(){

    if (state_ != kUdpConnectSynSend || trySendConnectSynTimes >= 15) {

        if (trySendConnectSynTimes >= 15) {
            LOG_INFO << "client trySendConnectSynTimes >= 15";
            LOG_INFO << "user tcp ";

            Buffer sendbuf(32);
            EncodeUint16(&sendbuf, packet::tcp::kPacketHeadLength);
            EncodeUint8(&sendbuf, packet::tcp::KUseTcp);
            tcpConnectionPtr_->send(&sendbuf);
            udpAvailble_ = false;
            setState(kConnected);
            tkcpConnectionCallback_(shared_from_this());
        }
        return;
    }
    trySendConnectSynTimes++;
    LOG_DEBUG << "trySendConnectSynTimes " << trySendConnectSynTimes;
    Buffer sendbuf(8);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kConnectSyn);
    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
    connectSyncAckTimer_ = loop_->runAfter(1.0,
                                           boost::bind(&TkcpSession::sendConnectSyn, this));
}


void TkcpSession::tcpPingRequest() {
    Buffer sendbuf(8);
    EncodeUint16(&sendbuf, static_cast<uint16_t>(packet::tcp::kPacketHeadLength));
    EncodeUint8(&sendbuf, packet::tcp::kPingRequest);
    tcpConnectionPtr_->send(&sendbuf);
}

void TkcpSession::onTcpPingRequest(Buffer* buf) {
    buf->retrieve(packet::tcp::kPacketHeadLength);
    lastRecvTcpPingDataTime_ = loop_->pollReturnTime();

    Buffer sendbuf(8);

    EncodeUint16(&sendbuf, static_cast<uint16_t>(packet::tcp::kPacketHeadLength));
    EncodeUint8(&sendbuf, packet::tcp::kPingReply);
    tcpConnectionPtr_->send(&sendbuf);
}

void TkcpSession::onTcpPingReply(Buffer *buf) {
    buf->retrieve(packet::tcp::kPacketHeadLength);
    lastRecvTcpPingDataTime_ = loop_->pollReturnTime();
}

void TkcpSession::onTcpData(Buffer* buf) {
    assert(state_ == kConnected);
    assert(udpAvailble_ == false);
    uint16_t len = DecodeUint16(buf);
    buf->retrieveInt8();
    size_t dataSize = len - packet::tcp::kPacketHeadLength;
    Buffer message(dataSize);
    message.append(buf->peek(), dataSize);
    buf->retrieve(dataSize);
    tkcpMessageCallback_(shared_from_this(), &message);
}

void TkcpSession::onUseTcp(Buffer* buf) {
    buf->retrieve(packet::tcp::KUseTcp);
    assert(state_ == kTcpConnected);
    udpAvailble_ = false;
    setState(kConnected);
    tkcpConnectionCallback_(shared_from_this());
}

void TkcpSession::ConnectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == kConnected) {

        loop_->cancel(udpPingTimer_);
        loop_->cancel(tcpPingTimer_);
        loop_->cancel(kcpUpdateTimer_);

        setState(kDisconnected);
        tkcpConnectionCallback_(shared_from_this());
    }
    tcpConnectionPtr_.reset();
    if (kcpcb_ != NULL) {
        ikcp_release(kcpcb_);
        kcpcb_ = NULL;
    }
}

void TkcpSession::Shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        tcpConnectionPtr_->shutdown();
    }
}

void TkcpSession::forceClose() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        tcpConnectionPtr_->forceClose();
    }
}

}
}





