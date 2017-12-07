
#include <boost/bind.hpp>

#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>


#include "ReliableConnection.h"
#include "ReliableUdpConnection.h"




using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::rudp;

void muduo::net::rudp::DefaultConnectionCallback(const ReliableUdpConnectionPtr& conn) {
    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
              << conn->peerAddress().toIpPort() << " -> "
              << (conn->Connected() ? "UP" : "DOWN");
}

void muduo::net::rudp::DefaultMessageCallback(const ReliableUdpConnectionPtr&,
                            Buffer* buf) {
    buf->retrieveAll();
}


static uint16_t getProtocolPacketLength(uint16_t mssSize)
{
    const uint16_t mateInfoSize =
        DATAGRAM_CONV_SIZEOF
        + DATAGAM_CRC_SIZEOF
        + DATAGRAM_HEAD_SIZEOF
        + DATAGRAM_CMD_SIZEOF
        + DATAGRAM_DATA_HEAD_SIZEOF;
    assert(mssSize > mateInfoSize);
    return std::min<uint16_t>(static_cast<uint16_t>(mssSize - mateInfoSize), static_cast<uint16_t>(DATAGRAM_PACK_SIZE_V6));
}


ReliableUdpConnection::ReliableUdpConnection(EventLoop* loop,
                                                   const string& name,
                                                   const InetAddress& localAddr,
                                                   const InetAddress& peerAddr,
                                                   uint16_t maxWindowSize,
                                                   uint16_t mssSize)
    : loop_(CHECK_NOTNULL(loop)),
      name_(name),
      state_(kConnecting),
      reliable_(new ReliableConnection(loop_, maxWindowSize, mssSize, getProtocolPacketLength(mssSize))),
      localAddr_(localAddr),
      peerAddr_(peerAddr) {

    LOG_DEBUG << "ReliableUdpConnection::ctor[" << name_ << "] at " << this;
    LOG_DEBUG << "ReliableUdpConnection::ctor[" << name_ << "] at " << this;
    reliable_->setPacketHandler(boost::bind(&ReliableUdpConnection::onReliableConnectionMessage, this, _1, _2));
    reliable_->setFlushHandler(boost::bind(&ReliableUdpConnection::onReliableConnectionFlush, this, _1, _2));
}

ReliableUdpConnection::~ReliableUdpConnection() {
    LOG_DEBUG << "ReliableUdpConnection::dtor[" << name_ << "] at " << this;
    assert(state_ == kDisconnected);
}


void ReliableUdpConnection::Send(const void* message, int len) {
    Send(StringPiece(static_cast<const char*>(message), len));
}

void ReliableUdpConnection::Send(const StringPiece& message) {
    if (state_ == KConnected) {
        if (loop_->isInLoopThread()) {
            sendToInLoop(message);
        } else {
            loop_->queueInLoop(boost::bind(&ReliableUdpConnection::sendToInLoop, shared_from_this(),
                               message.as_string()));
        }
    }
}


void ReliableUdpConnection::Send(Buffer* message) {
    if (state_ == KConnected) {
        if (loop_->isInLoopThread()) {
            sendToInLoop(message->peek(), message->readableBytes());
            message->retrieveAll();
        } else {
            loop_->queueInLoop(boost::bind(&ReliableUdpConnection::sendToInLoop, shared_from_this(),
                               message->retrieveAllAsString()));
        }
    }
}


void ReliableUdpConnection::sendToInLoop(const void* message, size_t len) {
    loop_->assertInLoopThread();

    if (state_ == kDisconnected) {
        LOG_WARN << "disconnected, give up writing";
        return;
    }

    reliable_->send(message, len);
}

void ReliableUdpConnection::sendToInLoop(const StringPiece& message) {
    sendToInLoop(message.data(), message.size());
}


void ReliableUdpConnection::setRtoBound(uint32_t minRto, uint32_t maxRto) {
    reliable_->setRtoBound(minRto, maxRto);
}

void ReliableUdpConnection::setAckDelay(bool delay, uint32_t delayMills) {
    reliable_->setAckDelay(delay, delayMills);
}

void ReliableUdpConnection::setRedundant(uint16_t n) {
    reliable_->setRedundant(n);
}

void ReliableUdpConnection::Process(const void* message, size_t len, const InetAddress& address) {
    if (state_ == KConnected || state_ == kConnecting) {
        if (loop_->isInLoopThread()) {
            precessInLoop(message, len, address);
        } else {
            loop_->queueInLoop(boost::bind(&ReliableUdpConnection::precessInLoop,
                                           shared_from_this(),
                                           string(static_cast<const char*>(message), len),
                                           address));
        }
    }
}

void ReliableUdpConnection::precessInLoop(const void* message, size_t len, const InetAddress& address) {
    loop_->assertInLoopThread();
    if (state_ == kDisconnecting || state_ == kDisconnected) {
        LOG_WARN << "no connected, give up precess";
        return;
    }
    peerAddr_ = address;
    reliable_->process(message, len);

}

void ReliableUdpConnection::precessInLoop(const StringPiece& message, const InetAddress& address){
    precessInLoop(message.data(), message.size(), address);
}


void ReliableUdpConnection::onReliableConnectionMessage(const void* message, std::size_t len) {
    loop_->assertInLoopThread();
    if (state_ == KConnected) {
        inputBuffer_.append(message, len);
        messageCallback_(shared_from_this(), &inputBuffer_);
    }

    if (state_ == kConnecting) {
        reliable_->send(message, len);
        setState(KConnected);
        loop_->cancel(connectEstabliseTimeId_);
        establishCallback_(shared_from_this(), true);
        connectionCallback_(shared_from_this());
    }
}

void ReliableUdpConnection::ConnectEstablished(const StringPiece& handshakeMessage) {
    ConnectEstablished(handshakeMessage.data(), handshakeMessage.size());
}

void ReliableUdpConnection::ConnectEstablished(const void* handshakeMessage, std::size_t len) {
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    reliable_->establish();
    reliable_->process(handshakeMessage, len);
    connectEstabliseTimeId_ = loop_->runAfter(10, boost::bind(&ReliableUdpConnection::onConnectionEstablishTimeout, shared_from_this()));
}

void ReliableUdpConnection::ConnectDestroyed() {
    loop_->assertInLoopThread();

    if (state_ == KConnected || state_ == kDisconnecting) {
        setState(kDisconnected);
        reliable_->destroy();
    }
}

void ReliableUdpConnection::Close() {
    if (state_ == KConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(boost::bind(&ReliableUdpConnection::closeInLoop, shared_from_this()));
    }
}

void ReliableUdpConnection::closeInLoop() {
    loop_->assertInLoopThread();

    assert(state_ == kDisconnecting);
    ReliableUdpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);
    closeCallback_(guardThis);
}

void ReliableUdpConnection::onReliableConnectionFlush(const void* data, std::size_t len) {
    connectionFlushCallback_(peerAddr_, data, len);
}

void ReliableUdpConnection::onConnectionEstablishTimeout() {
    if (state_ == kConnecting) {
        LOG_DEBUG << " " << name_ << " at " << this;
        setState(kDisconnected);
        reliable_->destroy();
        establishCallback_(shared_from_this(), false);
    }
}

