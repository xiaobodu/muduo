#pragma once


#include <stddef.h>

#include <map>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Types.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpServer.h>
#include <boost/unordered_map.hpp>

#include "TkcpConnection.h"
#include "TkcpCallback.h"



namespace muduo {

namespace net {

class UdpServerSocket;
typedef boost::shared_ptr<UdpServerSocket> UdpServerSocketPtr;

class TkcpServer : public boost::noncopyable {
    public:
        TkcpServer(EventLoop* loop,
                   const InetAddress& listenAddr,
                   const string& nameArg,
                   const int redundant = 0);
        ~TkcpServer();

        const InetAddress& listenAddress() const { return listenAddress_; }

        const string& name() const { return name_; }
        EventLoop* getLoop() const { return loop_; }

        void Start();

        void SetTkcpConnectionCallback(const TkcpConnectionCallback& cb) {
            tkcpConnectionCallback_ = cb;
        }

        void SetTkcpMessageCallback(const TkcpMessageCallback& cb) {
            tkcpMessageCallback_ = cb;
        }

    public:
    private:
        void newTcpConnection(const TcpConnectionPtr& conn);

        void removeTckpSession(const TkcpConnectionPtr& sess);
        void removeTckpSessionInLoop(const TkcpConnectionPtr& sess);

        void onUdpMessage(const UdpServerSocketPtr& socket, Buffer* buf, Timestamp time,
                                     const InetAddress& peerAddress);
        int outPutUdpMessage(const TkcpConnectionPtr& sess, const char* buf, size_t len);

    private:

    typedef boost::unordered_map<uint32_t, TkcpConnectionPtr> ConnectionMap;
    EventLoop* loop_;
    const InetAddress listenAddress_;
    const string name_;
    int redundant_;

    TkcpConnectionCallback tkcpConnectionCallback_;
    TkcpMessageCallback tkcpMessageCallback_;

    AtomicInt32 started_;

    uint32_t nextConv_;
    ConnectionMap connections_;

    UdpServerSocketPtr socket_;
    TcpServer tcpserver_;
};


}

}
