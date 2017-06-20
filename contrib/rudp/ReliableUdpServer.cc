
#include <assert.h>
#include <stdlib.h>

#include <boost/bind.hpp>
#include <boost/crc.hpp>
#include <boost/endian/arithmetic.hpp>
#include <boost/random/random_device.hpp>


#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>

#include "ReliableDefine.h"
#include "ReliableUdpServer.h"
#include "Callbacks.h"




using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::rudp;


ReliableUdpServer::ReliableUdpServer(EventLoop* loop,
                                     const InetAddress& listenAddr,
                                     const string& nameArg,
                                     uint32_t serverId)
    : loop_(CHECK_NOTNULL(loop)),
      listenAddr_(listenAddr),
      ipPort_(listenAddr_.toIpPort()),
      name_(nameArg),
      serverId_(serverId),
      numSockets_(1),
      mssSize_(DATAGRAM_MSS_V4),
      maxWindowSize_(64),
      random_(boost::random::random_device()()),
      threadPool_(new EventLoopThreadPool(loop_, name_)),
      connectionCallback_(DefaultConnectionCallback),
      messageCallback_(DefaultMessageCallback) {
}

ReliableUdpServer::~ReliableUdpServer() {
    loop_->assertInLoopThread();
    LOG_DEBUG << "ReliableUdpServer::~ReliableUdpServer [" << name_ << "] destructing";
    mutex_.lock();

    for (auto iter(connections_.begin());
            iter != connections_.end(); ++iter) {
        auto conn = iter->second;
        iter->second.reset();
        conn->getLoop()->runInLoop(
                boost::bind(&ReliableUdpConnection::ConnectDestroyed, conn));
        conn.reset();
    }
    mutex_.unlock();
}

void ReliableUdpServer::setThreadNum(int numThreads) {
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void ReliableUdpServer::setSocketNum(int socketNum) {
    assert(socketNum >= 1);
    numSockets_ = socketNum;
}

void ReliableUdpServer::setMssSize(uint16_t mssSize) {
    mssSize_ = mssSize;
}

void ReliableUdpServer::setMaxWindowSize(uint16_t maxWindowSize) {
    assert(maxWindowSize >= 1);
    maxWindowSize_ = maxWindowSize;
}

void ReliableUdpServer::Start() {
    if (started_.getAndSet(1) == 0) {
        threadPool_->start(threadInitCallback_);
        for (int i = 0; i < numSockets_; ++i) {
            char buf[name_.size() + 32];
            ::snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
            auto socket = UdpServerSocket::MakeUdpServerSocket(threadPool_->getNextLoop(), listenAddr_, buf);
            socket->SetMessageCallback(boost::bind(&ReliableUdpServer::handleUdpMessage, this, _1, _2, _3, _4));
            sockets_.push_back(socket);
            socket->Start();
        }
    }
}

void ReliableUdpServer::handleUdpMessage(const UdpServerSocketPtr& socket,
        Buffer* buf, Timestamp, const InetAddress& address) {
    uint64_t convId;
    if (buf->readableBytes() < sizeof(convId)) {
        return;
    }

    DatagramDeserialize(convId, buf->peek(), 0);

    if ((convId >> 32) != serverId_) {
        return;
    }

    if (!checkCrcchecksum(buf->peek(), buf->readableBytes())) {
        return;
    }
    std::size_t processBytes = buf->readableBytes() - sizeof(convId) - sizeof(uint16_t);

    uint32_t index = static_cast<uint32_t>(convId & 0xffffffff);

    if (index == 0) {
        if (loop_->isInLoopThread()) {
            newConnectionInLoop(socket, buf->peek() + sizeof(convId) , processBytes, address);
        } else {
            loop_->queueInLoop(boost::bind(&ReliableUdpServer::newConnectionInLoop, shared_from_this(),
                        socket, string(buf->peek() + sizeof(convId) , processBytes), address));
        }
        return;
    }

    ReliableUdpConnectionPtr conn;
    {
        MutexLockGuard lock(mutex_);
        auto connIter = connections_.find(convId);
        if (connIter != connections_.end()) {
            conn = connIter->second;
        }
    }

    if (!conn) {
        onConnectionReset(convId, socket, address);
        return;
    }



    if (processBytes == 0) {
        conn->Close();
    }

    conn->Process(buf->peek() + sizeof(convId), processBytes, address);
}

bool ReliableUdpServer::checkCrcchecksum(const void* buffer, std::size_t n) {
    uint16_t checksum;

    if (n >= sizeof(checksum)) {
        std::memcpy(&checksum,
                static_cast<const char*>(buffer) + n - sizeof(checksum),
                sizeof(checksum));
        boost::crc_16_type crc16;
        crc16.process_bytes(buffer, n - sizeof(checksum));
        return crc16.checksum() == checksum;
    }
    return false;
}

void ReliableUdpServer::onConnectionReset(uint64_t convId, const UdpServerSocketPtr& socket, const InetAddress& address) {
    boost::crc_16_type crc16;
    char buf[DATAGRAM_CONV_SIZEOF + DATAGAM_CRC_SIZEOF];
    std::size_t index = 0;

    index = DatagramSerialize(convId, buf, index);
    crc16.process_bytes(buf, index);

    uint16_t checksum = crc16.checksum();
    index = DatagramSerialize(checksum, buf, index);

    socket->SendTo(buf, index, address);
}

void ReliableUdpServer::onConnectionFlush(uint64_t convId, const UdpServerSocketPtr& socket,
        const InetAddress& address, const void* data, std::size_t len) {
    char buf[DATAGRAM_CONV_SIZEOF + DATAGAM_CRC_SIZEOF + len];
    std::size_t index = 0;

    boost::crc_16_type crc16;

    index = DatagramSerialize(convId, buf, index);
    index = DatagramSerialize(data, len,  buf, index);
    crc16.process_bytes(buf, index);
    uint16_t checksum = crc16.checksum();
    index = DatagramSerialize(checksum, buf, index);

    socket->SendTo(buf, index, address);
}

void ReliableUdpServer::removeConnection(uint64_t convId, const ReliableUdpConnectionPtr& conn) {
    if (loop_->isInLoopThread()) {
        removeConnectionInLoop(convId, conn);
    } else {
        loop_->queueInLoop(boost::bind(&ReliableUdpServer::removeConnectionInLoop, this, convId, conn));
    }
}

void ReliableUdpServer::removeConnectionInLoop(uint64_t convId, const ReliableUdpConnectionPtr& conn) {

    uint32_t index = static_cast<uint32_t>(convId & 0xffffffff);
    size_t n = convIndexs_.erase(index);
    (void)n;
    assert(n == 1);
    {
        MutexLockGuard lock(mutex_);
        n = connections_.erase(convId);
        assert(n == 1);
    }
    auto ioLoop = conn->getLoop();
    ioLoop->queueInLoop(boost::bind(&ReliableUdpConnection::ConnectDestroyed, conn));
}


uint64_t ReliableUdpServer::genConvId() {
    uint32_t index = random_();
    auto iter = convIndexs_.lower_bound(index);
    while (iter != convIndexs_.end() && *iter == index) {
        ++iter;
        ++index;
    }

    uint64_t convId = (uint64_t(serverId_) << 32) | index;
    return convId;
}

void ReliableUdpServer::newConnectionInLoop(const UdpServerSocketPtr& socket,
        const StringPiece& message, const InetAddress& address) {
    newConnectionInLoop(socket, message.data(), message.size(), address);
}

void ReliableUdpServer::newConnectionInLoop(const UdpServerSocketPtr& socket,
        const void* message, std::size_t len , const InetAddress& address) {

    if (inConnectingIpPortSet_.find(address.toIpPort()) != inConnectingIpPortSet_.end()) {
        return;
    }
    inConnectingIpPortSet_.insert(address.toIpPort());

    uint64_t convId = genConvId();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%ld", ipPort_.c_str(), convId);
    string connName = name_ + buf;

    LOG_INFO << "ReliableUdpServer newConnection [" << name_
             << "] - new connection [" << connName
             << "] from " << address.toIpPort();

    ReliableUdpConnectionPtr conn(new ReliableUdpConnection(socket->getLoop(),
                connName, listenAddr_, address, maxWindowSize_, mssSize_));
    conn->setConnectionCallback(connectionCallback_);
    conn->SetMessageCallback(messageCallback_);
    conn->setCloseCallback(boost::bind(&ReliableUdpServer::removeConnection, shared_from_this(), convId, _1));
    conn->setConnectionFlushCallback(boost::bind(&ReliableUdpServer::onConnectionFlush, shared_from_this(),
                convId, socket, _1, _2, _3));
    conn->setEstablishCallback(boost::bind(&ReliableUdpServer::onConnectionEstablishCallback, shared_from_this(), convId, address,  _1, _2));
    conn->setAckDelay(true, 1);

    {
        MutexLockGuard lock(mutex_);
        connections_[convId] = conn;
    }

    if (socket->getLoop()->isInLoopThread()) {
        conn->ConnectEstablished(message, len);
    } else {
        socket->getLoop()->queueInLoop(boost::bind(&ReliableUdpConnection::ConnectEstablished,
                    conn, string(static_cast<const char*>(message), static_cast<int>(len))));
    }

}

void ReliableUdpServer::onConnectionEstablishCallback(uint64_t convId, const InetAddress& address,  const ReliableUdpConnectionPtr& conn, bool isSuccess) {

    if (loop_->isInLoopThread()) {
        onConnectionEstablishCallbackInLoop(convId, address, conn, isSuccess);
    } else {
        loop_->queueInLoop(boost::bind(&ReliableUdpServer::onConnectionEstablishCallbackInLoop, shared_from_this(), convId, address, conn, isSuccess));
    }
}

void ReliableUdpServer::onConnectionEstablishCallbackInLoop(uint64_t convId, const InetAddress& addres, const ReliableUdpConnectionPtr& conn, bool isSuccess) {

    if (!isSuccess) {
       MutexLockGuard lock(mutex_);
       connections_.erase(convId);
    }
    inConnectingIpPortSet_.erase(addres.toIpPort());
}

