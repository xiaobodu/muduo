

#include <muduo/net/EventLoop.h>
#include <muduo/net/Channel.h>
#include <muduo/net/SocketsOps.h>


#include "UdpServerSocket.h"


using namespace  muduo;
using namespace  muduo::net;


static void DefaultMessageCallback(const UdpServerSocketPtr& socket, Buffer* buf, Timestamp receiveTime, const InetAddress& address) {
    (void)buf;
    LOG_DEBUG << "receive udp message from " << address.toIpPort();
}


UdpServerSocket::UdpServerSocket(EventLoop* loop,
        const InetAddress& listenAddr,
        const string& nameArg,
        size_t maxPacketSize)
    : loop_(CHECK_NOTNULL(loop)),
    listenAddr_(listenAddr),
    name_(nameArg),
    sendBufferSize_(0),
    receiveBufferSize_(0),
    maxPacketSize_(maxPacketSize),
    readBuf_(maxPacketSize+ 1),
    messageCallback_(DefaultMessageCallback),
    writeBlocked_(false) {

    LOG_DEBUG << "UdpServerSocket::ctor[" << name_ << "] at " << this;
}


UdpServerSocket::~UdpServerSocket() {
    LOG_DEBUG << "UdpServerSocket::dtor[" << name_ << "] at" << this;
    channel_->disableAll();
    channel_->remove();
}

UdpServerSocketPtr UdpServerSocket::MakeUdpServerSocket(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg, size_t maxPacketSize) {
    UdpServerSocketPtr socket(new UdpServerSocket(loop, listenAddr, nameArg, maxPacketSize));
    return socket;
}

void UdpServerSocket::Start() {
    if (started_.getAndAdd(1) == 0) {
        loop_->runInLoop(boost::bind(&UdpServerSocket::startInLoop, shared_from_this()));
    }
}

void UdpServerSocket::startInLoop() {
    assert(!socket_.IsConnected());
    assert(!channel_.get());

    socket_.AllowTosWithLowDelay();
    socket_.AllowAddressReuse();
    socket_.AllowPortResuse();

    int result = socket_.Bind(listenAddr_);
    if (result < 0) {
        LOG_SYSFATAL << "UdpServerSocket::Listen";
    }

    if (sendBufferSize_ > 0) {
        socket_.SetSendBufferSize(sendBufferSize_);
    }

    if (receiveBufferSize_ > 0) {
        socket_.SetReceiveBufferSize(receiveBufferSize_);
    }

    assert(socket_.IsConnected());
    channel_.reset(new Channel(loop_, socket_.sockfd()));
    channel_->setReadCallback(boost::bind(&UdpServerSocket::handleRead,   this, _1));
    channel_->setWriteCallback(boost::bind(&UdpServerSocket::handleWrite, this));
    channel_->setErrorCallback(boost::bind(&UdpServerSocket::handleError, this));
    channel_->enableReading();
}

void UdpServerSocket::SendTo(const void* message, size_t len,
                       const InetAddress& address) {
    SendTo(StringPiece(static_cast<const char*>(message), static_cast<int>(len)), address);
}

void UdpServerSocket::SendTo(const StringPiece& message, const InetAddress& address) {
    if (loop_->isInLoopThread()) {
        sendToInLoop(message, address);
    } else {
        loop_->queueInLoop(boost::bind(&UdpServerSocket::sendToInLoop, shared_from_this(),
                    message.as_string(), address));
    }
}

void UdpServerSocket::SendTo(Buffer* message, const InetAddress& address) {
    if (loop_->isInLoopThread()) {
        sendToInLoop(message->peek(), message->readableBytes(), address);
        message->retrieveAll();
    } else {
        loop_->queueInLoop(boost::bind(&UdpServerSocket::sendToInLoop, shared_from_this(),
                    message->retrieveAllAsString(), address));
    }
}

void UdpServerSocket::handleRead(Timestamp receiveTime) {
    assert(socket_.IsConnected());
    assert(readBuf_.writableBytes() > 0);

    InetAddress address;
    ssize_t nr = socket_.RecvFrom(readBuf_.beginWrite(),
                              readBuf_.writableBytes(), &address);

    if (nr < 0) {
        errno = static_cast<int>(nr);
        LOG_SYSERR << "UdpServerSocket::HandleRead";
        handleError();
    } else if (muduo::implicit_cast<size_t>(nr) <= maxPacketSize_) {
        readBuf_.hasWritten(nr);

        if (messageCallback_) {
            messageCallback_(shared_from_this(), &readBuf_, receiveTime, address);
        }

        readBuf_.retrieveAll();
    } else {
        LOG_WARN << "received packet size is too large from " << address.toIpPort()
                 << ", datagram has been truncated";
    }
}

void UdpServerSocket::handleWrite() {
    setWritable();

    auto packetIterator = queuePackets_.begin();
    while (!isWriteBlocked() && packetIterator != queuePackets_.end()) {
        if (handleWrite(&(*packetIterator)) >= 0) {
            packetIterator = queuePackets_.erase(packetIterator);
        } else {
            ++packetIterator;
        }
    }

    if (queuePackets_.empty()) {
        channel_->disableWriting();
    }
}

ssize_t UdpServerSocket::handleWrite(QueuePacket* packet) {
    assert(!isWriteBlocked());


    ssize_t nw = socket_.SendTo(packet->Data, packet->Size, packet->PeerAddr);
    if (nw < 0) {
        if (nw == EAGAIN || nw == EWOULDBLOCK) {
            setWriteBlocked();
        }

        errno = static_cast<int>(nw);
        LOG_SYSERR << "UdpServerSocket::HanleWrite";

        handleError();
    }
    return nw;
}

void UdpServerSocket::sendToInLoop(const StringPiece& message, const InetAddress& address) {
    sendToInLoop(message.data(), message.size(), address);
}

void UdpServerSocket::sendToInLoop(const void* message, size_t len, const InetAddress& address) {
    loop_->assertInLoopThread();

    assert(socket_.IsConnected());

    if (isWriteBlocked()) {
        queuePackets_.push_back(new QueuePacket(message, len, address));
        return;
    }

    ssize_t nw = socket_.SendTo(message, len, address);

    if (nw < 0) {
        if (nw == EAGAIN || nw == EWOULDBLOCK) {
            setWriteBlocked();
            channel_->enableWriting();
            queuePackets_.push_back(new QueuePacket(message, len, address));
        }

        errno = static_cast<int>(nw);
        LOG_SYSERR << "UdpServerSocket::SendToInLoop";

        handleError();
    }
}

void UdpServerSocket::handleError() {
    int err = muduo::net::sockets::getSocketError(channel_->fd());
    LOG_ERROR << "UdpServerSocket::HandleError [fd: " << socket_.sockfd()
              << "] - SO_ERROR = " << err << " " << muduo::strerror_tl(err);
}
