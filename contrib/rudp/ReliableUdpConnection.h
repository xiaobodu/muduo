#pragma once

#include <atomic>

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Atomic.h>
#include <muduo/net/TimerId.h>



#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>


#include "ReliableDefine.h"
#include "ReliableFlusher.h"
#include "Callbacks.h"


namespace muduo {
namespace net {

class EventLoop;

namespace rudp {

class ReliableConnection;
class ReliableUdpConnection;
typedef boost::shared_ptr<ReliableUdpConnection> ReliableUdpConnectionPtr;

typedef boost::function<void(const InetAddress&, const void*, std::size_t)> ConnectionFlushCallback;

// 一个UDP连接，可以收发proto消息
class ReliableUdpConnection : public boost::enable_shared_from_this<ReliableUdpConnection>, boost::noncopyable
{
public:
	ReliableUdpConnection(EventLoop* loop,
                          const string& name,
                          const InetAddress& localAddr,
                          const InetAddress& peerAddr,
                          uint16_t maxWindowSize, uint16_t mssSize);

	~ReliableUdpConnection();

    EventLoop* getLoop() const { return loop_; }
    const string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool Connected() const { return state_ == KConnected; }
    bool Disconnected() const { return state_ == kDisconnected; }

    void Send(const void* message, int len);
    void Send(const StringPiece& message);
    void Send(Buffer* message);

    void setContext(const boost::any& context) {
        context_ = context;
    }

    const boost::any& getContext() const {
        return context_;
    }

    boost::any* getMutableContext() {
        return &context_;
    }


    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    void SetMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    void setCloseCallback(const CloseCallback& cb) {
        closeCallback_ = cb;
    }

    void setEstablishCallback(const EstablishCallback& cb) {
        establishCallback_ = cb;
    }

    void ConnectEstablished(const StringPiece& handshakeMessage);
    void ConnectEstablished(const void* handshakeMessage, std::size_t len);

    void ConnectDestroyed();

	// 底层输入
	void Process(const void* data, std::size_t n, const InetAddress& address);

	// 设置rto范围
	void setRtoBound(uint32_t minRto, uint32_t maxRto);

	// 设置延迟模式，ack包会延迟发送
	void setAckDelay(bool delay, uint32_t delayMills = DATAGRAM_DELAY_TIME);

	// 设置是否冗余发送
	void setRedundant(uint16_t n);

	// 关闭连接
	void Close();
	// 刷新输出
	void setConnectionFlushCallback(const ConnectionFlushCallback& cb) {
        connectionFlushCallback_ = cb;
    }
private:
    enum StateE {
        kConnecting = 0,
        KConnected = 1,
        kDisconnecting = 2,
        kDisconnected = 3,
    };
private:
    void onReliableConnectionMessage(const void* message, std::size_t len);
    void onReliableConnectionFlush(const void* data, std::size_t len);

    void onConnectionEstablishTimeout();

    void sendToInLoop(const void* message, size_t len);
    void sendToInLoop(const StringPiece& message);
    void precessInLoop(const void* message, size_t len, const InetAddress& address);
    void precessInLoop(const StringPiece& message, const InetAddress& address);
    void setState(StateE s) { state_ = s; }

    void closeInLoop();
private:
    EventLoop* loop_;
    const string name_;
    StateE state_;

    boost::shared_ptr<ReliableConnection> reliable_;

	// 本端地址
    InetAddress localAddr_;
	// 远端地址
    InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;

    CloseCallback closeCallback_;
    EstablishCallback establishCallback_;
    ConnectionFlushCallback connectionFlushCallback_;

    Buffer inputBuffer_;

    boost::any context_;

    TimerId connectEstabliseTimeId_;
};

}
}
}
