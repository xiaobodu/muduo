

#include <muduo/net/EventLoop.h>
#include <muduo/net/Channel.h>
#include <muduo/net/SocketsOps.h>


#include "UdpClientSocket.h"


using namespace  muduo;
using namespace  muduo::net;
using namespace muduo::net::rudp;


static void DefaultMessageCallback(Buffer* buf, Timestamp receiveTime) {
    (void)buf;
    LOG_DEBUG << "receive udp message";
}

static void DefaultConnectCallback(UdpClientSocket::ConnectResult result) {
    LOG_DEBUG << "connect result " << result;
}

static void DefaultDisconnectCallback(UdpClientSocket::DisconnectResutl result) {
    LOG_DEBUG << "disconnect result " << result;
}



UdpClientSocket::UdpClientSocket(EventLoop* loop, const string& nameArg,  size_t maxPacketSize)
    : loop_(CHECK_NOTNULL(loop)),
      name_(nameArg),
      sendBufferSize_(0),
      receiveBufferSize_(0),
      maxPacketSize_(maxPacketSize),
      readBuf_(maxPacketSize_ + 1),
      messageCallback_(DefaultMessageCallback),
      connectCallback_(DefaultConnectCallback),
      disconnectCallback_(DefaultDisconnectCallback),
      writeBlocked_(false) {
    LOG_DEBUG << "UdpClientSocket::ctor[" << name_ <<  "] at " << this;
}

UdpClientSocket::~UdpClientSocket() {
    LOG_DEBUG << "UdpClientSocket::dtor[" << name_ << "] at " << this;
    assert(!socket_.IsConnected());
    assert(!channel_);
}


UdpClientSocketPtr UdpClientSocket::MakeUdpClientSocket(EventLoop* loop, const string& nameArg, size_t maxPacketSize) {
    UdpClientSocketPtr socket(new UdpClientSocket(loop, nameArg, maxPacketSize));
    return socket;
}



void UdpClientSocket::Connect(const InetAddress& address) {
    if (connected.getAndSet(1) == 0) {
        loop_->runInLoop(boost::bind(&UdpClientSocket::connectInLoop, shared_from_this(), address));
    }
}

void UdpClientSocket::connectInLoop(const InetAddress& address) {
    loop_->assertInLoopThread();
    assert(!socket_.IsConnected());

    int result = socket_.Connect(address);
    if (result < 0) {
        LOG_SYSERR << "UdpClientSocket::connectInLoop - connect to " << address.toIpPort()
                   << " failed, "
                   << " error = " << muduo::strerror_tl(result);
        connectCallback_(kConnectFailure);
    }

    assert(socket_.IsConnected());
    assert(!channel_);
    channel_.reset(new Channel(loop_, socket_.sockfd()));
    channel_->setReadCallback(boost::bind(&UdpClientSocket::HandleRead, this, _1));
    channel_->setWriteCallback(boost::bind(&UdpClientSocket::HandleWrite, this));
    channel_->setErrorCallback(boost::bind(&UdpClientSocket::HandleError, this));
    channel_->enableReading();

    connectCallback_(kConnectSuccess);
}


void UdpClientSocket::Disconnect() {
    if (connected.getAndSet(2) == 1) {
        loop_->runInLoop(boost::bind(&UdpClientSocket::disconnectInLoop, shared_from_this()));
    }
}


void UdpClientSocket::disconnectInLoop() {
    loop_->assertInLoopThread();
    assert(channel_);

    channel_->disableAll();
    channel_->remove();
    channel_.reset();

    if (socket_.IsConnected()) {
        socket_.Close();
    }

    disconnectCallback_(kDisconnectSuccess);
    connectCallback_ = DefaultConnectCallback;
    messageCallback_ = DefaultMessageCallback;
    disconnectCallback_ = DefaultDisconnectCallback;
}


void UdpClientSocket::Write(const void* message, size_t len) {
    Write(StringPiece(static_cast<const char*>(message), static_cast<int>(len)));
}

void UdpClientSocket::Write(const StringPiece& message) {
    if (loop_->isInLoopThread()) {
        WriteInLoop(message);
    } else {
        loop_->queueInLoop(boost::bind(&UdpClientSocket::WriteInLoop, shared_from_this(), message.as_string()));
    }
}


void UdpClientSocket::Write(Buffer* message) {
    if (loop_->isInLoopThread()) {
        WriteInLoop(message->peek(), message->readableBytes());
        message->retrieveAll();
    } else {
        loop_->queueInLoop(boost::bind(&UdpClientSocket::WriteInLoop, shared_from_this(),
                    message->retrieveAllAsString()));
    }
}


void UdpClientSocket::HandleRead(Timestamp receiveTime) {
    assert(socket_.IsConnected());
    assert(readBuf_.writableBytes() > 0);

    InetAddress address;
    ssize_t nr = socket_.RecvFrom(readBuf_.beginWrite(),
                              readBuf_.writableBytes(), &address);

    if (nr < 0) {
        errno = static_cast<int>(nr);
        LOG_SYSERR << "UdpClientSocket::HandleRead";
        HandleError();
    } else if (muduo::implicit_cast<size_t>(nr) <= maxPacketSize_) {
        readBuf_.hasWritten(nr);

        if (messageCallback_) {
            messageCallback_(&readBuf_, receiveTime);
        }

        readBuf_.retrieveAll();
    } else {
        LOG_WARN << "received packet size is too large from " << address.toIpPort()
                 << ", datagram has been truncated";
    }
}

void UdpClientSocket::HandleWrite() {
    SetWritable();

    auto packetIterator = queuePackets_.begin();
    while (!IsWriteBlocked() && packetIterator != queuePackets_.end()) {
        if (HandleWrite(&(*packetIterator)) >= 0) {
            packetIterator = queuePackets_.erase(packetIterator);
        } else {
            ++packetIterator;
        }
    }

    if (queuePackets_.empty()) {
        channel_->disableWriting();
    }
}

ssize_t UdpClientSocket::HandleWrite(QueuePacket* packet) {
    assert(!IsWriteBlocked());


    ssize_t nw = socket_.Write(packet->Data, packet->Size);
    if (nw < 0) {
        if (nw == EAGAIN || nw == EWOULDBLOCK) {
            SetWriteBlocked();
        }

        errno = static_cast<int>(nw);
        LOG_SYSERR << "UdpClientSocket::HanleWrite";

        HandleError();
    }
    return nw;
}

void UdpClientSocket::WriteInLoop(const StringPiece& message) {
    WriteInLoop(message.data(), message.size());
}

void UdpClientSocket::WriteInLoop(const void* message, size_t len) {
    loop_->assertInLoopThread();

    assert(socket_.IsConnected());

    if (IsWriteBlocked()) {
        queuePackets_.push_back(new QueuePacket(message, len));
        return;
    }

    ssize_t nw = socket_.Write(message, len);

    if (nw < 0) {
        if (nw == EAGAIN || nw == EWOULDBLOCK) {
            SetWriteBlocked();
            channel_->enableWriting();
            queuePackets_.push_back(new QueuePacket(message, len));
        }
        errno = static_cast<int>(nw);
        LOG_SYSERR << "UdpClientSocket::WriteInLoop";

        HandleError();
    }
}

void UdpClientSocket::HandleError() {
    int err = muduo::net::sockets::getSocketError(channel_->fd());
    LOG_ERROR << "UdpClientSocket::HandleError [fd: " << socket_.sockfd()
              << "] - SO_ERROR = " << err << " " << muduo::strerror_tl(err);
}
