
#include <stddef.h>
#include <sys/time.h>

#include <boost/bind.hpp>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <stdint.h>
#include "TkcpConnection.h"
#include "Packet.h"
#include "Coding.h"
#include "Fec.h"
#include "ikcp.h"
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


void defaultTkcpConnectionCallback(const TkcpConnectionPtr& conn) {
    LOG_TRACE << "(tcp:" << conn->localTcpAddress().toIpPort()
              << ",udp:" << conn->localUdpAddress().toPort()
              << ")->" << "(tcp:" << conn->peerUdpAddress().toIpPort()
              << ",udp:" << conn->localUdpAddress().toPort()
              << ") is" << (conn->Connected() ? "UP" : "DOWN");
}

void defaultTkcpMessageCallback(const TkcpConnectionPtr&, Buffer* buf) {
    buf->retrieveAll();
}

TkcpConnection::TkcpConnection(uint32_t conv,
        const InetAddress& localUdpAddress,
        const InetAddress& peerUdpAddress,
        const TcpConnectionPtr& tcpConnectionPtr,
        const string& nameArg,
        const int redundant)
    :
      loop_(tcpConnectionPtr->getLoop()),
      conv_(conv),
      name_(nameArg),
      redundant_(redundant),
      tcpConnectionPtr_(tcpConnectionPtr),
      state_(kTcpConnected),
      peerUdpAddress_(peerUdpAddress),
      localUdpAddress_(localUdpAddress),
      kcpInited_(false),
      kcpcb_(NULL),
      kcpRecvBuf_(2048),
      trySendConnectSynTimes(0),
      udpAvailble_(true),
      kcpState_(0){

    fec_.reset(new Fec(redundant));
    fec_->setSendOutCallback(boost::bind(&TkcpConnection::onFecSendData, this, _1, _2));
    fec_->setRecvOutCallback(boost::bind(&TkcpConnection::onFecRecvData, this, _1, _2));

    LOG_DEBUG << "TkcpConnection::ctor[" << name_ << "] at" << this;
}

TkcpConnection::~TkcpConnection() {
    assert(kcpcb_ == NULL);

    LOG_DEBUG << "TkcpConnection::dtor[" << name_ << "] at" << this
              << " state=" << stateToString();
    assert(kcpcb_ == NULL);
    assert(state_ == kDisconnected);
}

const char* TkcpConnection::stateToString() const {
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


void TkcpConnection::Send(const void* data, int len) {
    Send(StringPiece(static_cast<const char*>(data), len));
}

void TkcpConnection::Send(const StringPiece& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            SendInLoop(message);
        } else {
            loop_->runInLoop(boost::bind(&TkcpConnection::SendInLoop,
                                         this,
                                         message.as_string()));
        }
    }
}

void TkcpConnection::Send(Buffer* buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            SendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        } else {
            loop_->runInLoop(boost::bind(&TkcpConnection::SendInLoop,
                                         this,
                                         buf->retrieveAllAsString()));
        }
    }
}

void TkcpConnection::SendInLoop(const StringPiece& message) {
    SendInLoop(message.data(), message.size());
}

void TkcpConnection::SendInLoop(const void *data, size_t len) {
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

void TkcpConnection::sendKcpMsg(const void *data, size_t len) {
    ikcp_send(kcpcb_, static_cast<const char*>(data), static_cast<int>(len));
    if (!(kcpState_ & KcpStateE::kUpdating)) {
        postImmediatelyUpdateKcp();
    }
}

void TkcpConnection::sendTcpMsg(const void *data, size_t len) {
    Buffer sendBuf;
    EncodeUint16(&sendBuf, static_cast<uint16_t>(len + packet::tcp::kPacketHeadLength));
    EncodeUint8(&sendBuf, packet::tcp::kData);
    sendBuf.append(data, len);
    tcpConnectionPtr_->send(&sendBuf);
}


int TkcpConnection::handleKcpOutput(const char* buf, int len) {
    fec_->Send(buf, static_cast<size_t>(len));
    return 0;
}

void TkcpConnection::onFecSendData(const char* data, size_t len) {

    if (udpOutputCallback_) {
        EncodeUint32(&udpSendBuf_, conv_);
        EncodeUint8(&udpSendBuf_, packet::udp::kData);
        udpSendBuf_.append(data, len);
        udpOutputCallback_(shared_from_this(), udpSendBuf_.peek(), udpSendBuf_.readableBytes());
        udpSendBuf_.retrieveAll();
    }
}

int TkcpConnection::kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user) {
    (void)kcp;
    TkcpConnection* sess = static_cast<TkcpConnection*>(user);
    return sess->handleKcpOutput(buf, len);
}

// safe
void TkcpConnection::InputUdpMessage(Buffer* buf, const InetAddress& peerAddress) {
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

    peerUdpAddress_ = peerAddress;
    uint32_t conv = DecodeUint32(buf);
    uint8_t packeId = DecodeUint8(buf);
    LOG_TRACE << "recv " << packet::udp::PacketIdToString(packeId) <<
                 " udpmsg from conv " << conv;
    switch (packeId) {
        case packet::udp::kData:
            onUdpData(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
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
            LOG_WARN << "unknown udp message " << packeId << " from " << peerAddress.toIpPort();
            forceClose();
    }
}

void TkcpConnection::onConnectSyn() {
    Buffer sendbuf(8);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kConnectSynAck);

    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpConnection::onConnectSyncAck() {
     if (state_ != kUdpConnectSynSend) {
        return;
    }
    initKcp(); // client
    udpPingTimer_ = loop_->runEvery(60, boost::bind(&TkcpConnection::udpPingRequest, this));
    tcpPingTimer_ = loop_->runAfter(120, boost::bind(&TkcpConnection::tcpPingRequest, this));
    Buffer sendbuf;
    packet::tcp::TransportMode mode;
    mode.packetId = packet::tcp::TcpPacketId::KTransportMode;
    mode.mode = packet::tcp::TransportMode::kUdp;
    mode.Encode(&sendbuf);
    tcpConnectionPtr_->send(&sendbuf);
    setState(kConnected);
    tkcpConnectionCallback_(shared_from_this());
}




void TkcpConnection::initKcp() {
    assert(kcpcb_ == NULL);

    assert(state_ == kTcpConnected || state_ == kUdpConnectSynSend);

    kcpcb_  = ikcp_create(conv_, this);
    ikcp_setoutput(kcpcb_, kcpOutput);
    ikcp_nodelay(kcpcb_, 1, 200, 2, 1);
    ikcp_wndsize(kcpcb_, 128, 128);
    ikcp_setmtu(kcpcb_, fec_->Mtu());


    nextKcpTime_ = ikcp_update(kcpcb_, TimestampToMillisecond(Timestamp::now()));
    updateKcpTimer_ = loop_->runEvery(MillisecondToSecond(50),
                    boost::bind(&TkcpConnection::onUpdateKcp, this));
}

void TkcpConnection::onPingRequest() {
    lastRecvUdpPingDataTime_ = loop_->pollReturnTime();
    Buffer sendbuf(8);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kPingReply);
    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpConnection::onPingReply() {
    lastRecvUdpPingDataTime_ = loop_->pollReturnTime();
}

void TkcpConnection::udpPingRequest() {
    Buffer sendbuf(8);
    EncodeUint32(&sendbuf, conv_);
    EncodeUint8(&sendbuf, packet::udp::kPingRequest);
    udpOutputCallback_(shared_from_this(), sendbuf.peek(), sendbuf.readableBytes());
}

void TkcpConnection::onUdpData(const char* buf, size_t len) {

    if (state_ != kConnected) {
        return;
    }

    fec_->Input(buf, len);
}

void TkcpConnection::onFecRecvData(const char* data, size_t len) {
    ikcp_input(kcpcb_, data, static_cast<int>(len));
    immediatelyUpdateKcp();
}


void TkcpConnection::SyncUdpConnectionInfo() {
    Buffer sendbuf(64);
    packet::tcp::UdpConnectionInfo connInfo;
    connInfo.packetId= packet::tcp::kUdpConnectionInfo;
    connInfo.conv = conv_;
    connInfo.Encode(&sendbuf);


    tcpConnectionPtr_->send(&sendbuf);
}


void TkcpConnection::onTcpConnection(const TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();
    assert(conn->disconnected());
    if (conn->disconnected()) {

        assert(state_ == kConnected || state_ == kTcpConnected || state_ == kUdpConnectSynSend || state_ == kDisconnecting);
        setState(kDisconnected);

        loop_->cancel(udpPingTimer_);
        loop_->cancel(tcpPingTimer_);
        loop_->cancel(updateKcpTimer_);

        TkcpConnectionPtr guardThis(shared_from_this());
        tkcpConnectionCallback_(guardThis);


        tkcpCloseCallback_(guardThis);
    }

}


void TkcpConnection::onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
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
                case packet::tcp::KTransportMode:
                    onTransportMode(buf);
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
                    buf->retrieveAll();
                    LOG_WARN << "unknown tcp message " << packeId << " from " << tcpConnectionPtr_->peerAddress().toIpPort();
                    forceClose();
            }
        } else {
            break;
        }
    }
}

void TkcpConnection::onUdpconnectionInfo(Buffer* buf) {
    packet::tcp::UdpConnectionInfo connInfo;
    connInfo.Decode(buf);
    conv_ = connInfo.conv;
    setState(kUdpConnectSynSend);
    sendConnectSyn();
}
void TkcpConnection::sendConnectSyn(){

    if (state_ != kUdpConnectSynSend || trySendConnectSynTimes >= 15) {

        if (trySendConnectSynTimes >= 15) {
            LOG_INFO << "client trySendConnectSynTimes >= 15";
            LOG_INFO << "user tcp ";

            packet::tcp::TransportMode mode;
            Buffer sendbuf(32);
            mode.packetId = packet::tcp::TcpPacketId::KTransportMode;
            mode.mode = packet::tcp::TransportMode::KTcp;
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
                                           boost::bind(&TkcpConnection::sendConnectSyn, this));
}


void TkcpConnection::tcpPingRequest() {
    Buffer sendbuf(8);
    EncodeUint16(&sendbuf, static_cast<uint16_t>(packet::tcp::kPacketHeadLength));
    EncodeUint8(&sendbuf, packet::tcp::kPingRequest);
    tcpConnectionPtr_->send(&sendbuf);
}

void TkcpConnection::onTcpPingRequest(Buffer* buf) {
    buf->retrieve(packet::tcp::kPacketHeadLength);
    lastRecvTcpPingDataTime_ = loop_->pollReturnTime();

    Buffer sendbuf(8);

    EncodeUint16(&sendbuf, static_cast<uint16_t>(packet::tcp::kPacketHeadLength));
    EncodeUint8(&sendbuf, packet::tcp::kPingReply);
    tcpConnectionPtr_->send(&sendbuf);
}

void TkcpConnection::onTcpPingReply(Buffer *buf) {
    buf->retrieve(packet::tcp::kPacketHeadLength);
    lastRecvTcpPingDataTime_ = loop_->pollReturnTime();
}

void TkcpConnection::onTcpData(Buffer* buf) {
    assert(state_ == kConnected);
    assert(udpAvailble_ == false);
    if (udpAvailble_) {
        return;
    }
    uint16_t len = DecodeUint16(buf);
    buf->retrieveInt8();
    size_t dataSize = len - packet::tcp::kPacketHeadLength;
    Buffer message(dataSize);
    message.append(buf->peek(), dataSize);
    buf->retrieve(dataSize);
    tkcpMessageCallback_(shared_from_this(), &message);
}

void TkcpConnection::onTransportMode(Buffer* buf) {
    packet::tcp::TransportMode mode;
    mode.Decode(buf);

    if (mode.mode == packet::tcp::TransportMode::kUdp) {
       initKcp();
    } else {
        udpAvailble_ = false;
    }

    setState(kConnected);
    tkcpConnectionCallback_(shared_from_this());
}

void TkcpConnection::ConnectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == kConnected) {

        loop_->cancel(udpPingTimer_);
        loop_->cancel(tcpPingTimer_);
        loop_->cancel(updateKcpTimer_ );

        setState(kDisconnected);
        tkcpConnectionCallback_(shared_from_this());
    }
    tcpConnectionPtr_.reset();
    if (kcpcb_ != NULL) {
        ikcp_release(kcpcb_);
        kcpcb_ = NULL;
    }
}

void TkcpConnection::Shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        tcpConnectionPtr_->shutdown();
    }
}

void TkcpConnection::forceClose() {
    if (state_ != kDisconnecting && state_ != kDisconnected) {
        setState(kDisconnecting);
        tcpConnectionPtr_->forceClose();
    }
}

void TkcpConnection::Close() {
    forceClose();
}

/*
            onUpdateKcp();
            void immediatelyUpdateKcp();
            void immediatelyUpdateKcp(uint32_t now);
            void postImmediatelyUpdateKcp();
            void updateKcp(uint32_t now, bool fromeTimer);

    int nr = ikcp_recv(kcpcb_, kcpRecvBuf_.beginWrite(), static_cast<int>(kcpRecvBuf_.writableBytes()));
    if (nr > 0) {
        kcpRecvBuf_.hasWritten(nr);
        tkcpMessageCallback_(shared_from_this(), &kcpRecvBuf_);
        kcpRecvBuf_.retrieveAll();
    } else if (nr == -3) {
        int size = ikcp_peeksize(kcpcb_);
        kcpRecvBuf_.ensureWritableBytes(static_cast<size_t>(size));
        nr = ikcp_recv(kcpcb_, kcpRecvBuf_.beginWrite(), static_cast<int>(kcpRecvBuf_.writableBytes()));
        if (nr > 0) {
            kcpRecvBuf_.hasWritten(nr);
            tkcpMessageCallback_(shared_from_this(), &kcpRecvBuf_);
            kcpRecvBuf_.retrieveAll();
        }
    }
*/


void TkcpConnection::onUpdateKcp() {
    uint32_t current = TimestampToMillisecond(Timestamp::now());
    if (current > nextKcpTime_) {
        updateKcp(current);
    }
}
void TkcpConnection::immediatelyUpdateKcp() {
    uint32_t current =TimestampToMillisecond(Timestamp::now());
    immediatelyUpdateKcp(current);
}

void TkcpConnection::immediatelyUpdateKcp(uint32_t current) {
    kcpState_ &= ~KcpStateE::kPosting;
    updateKcp(current);
}

void TkcpConnection::postImmediatelyUpdateKcp() {
    if (!(kcpState_ & KcpStateE::kPosting)) {
        kcpState_ |= KcpStateE::kPosting;
        loop_->queueInLoop(boost::bind(&TkcpConnection::immediatelyUpdateKcp, shared_from_this()));
    }
}

void TkcpConnection::updateKcp(uint32_t current) {
    kcpState_ |= KcpStateE::kUpdating;
    int nr = ikcp_recv(kcpcb_, kcpRecvBuf_.beginWrite(), static_cast<int>(kcpRecvBuf_.writableBytes()));;
    while (nr != -1) {
        if (nr > 0) {
            kcpRecvBuf_.hasWritten(nr);
            tkcpMessageCallback_(shared_from_this(), &kcpRecvBuf_);
            kcpRecvBuf_.retrieveAll();
        } else {
            int size = ikcp_peeksize(kcpcb_);
            kcpRecvBuf_.ensureWritableBytes(static_cast<size_t>(size));
        }
        nr = ikcp_recv(kcpcb_, kcpRecvBuf_.beginWrite(), static_cast<int>(kcpRecvBuf_.writableBytes()));
    }
    kcpState_ &= ~KcpStateE::kUpdating;

    nextKcpTime_ = ikcp_update(kcpcb_, current);
}

}
}


