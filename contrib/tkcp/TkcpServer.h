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

#include "TkcpSession.h"
#include "TkcpCallback.h"



namespace muduo {

namespace net {

class UdpService;
class TkcpServer : public boost::noncopyable {
    public:
        TkcpServer(EventLoop* loop,
                   const InetAddress& tcpListenAddr,
                   const InetAddress& udpListenAddr,
                   const string& nameArg);
        ~TkcpServer();

        const InetAddress& tcpListenAddress() const { return tcpListenAddress_; }
        const InetAddress& udpListenAddress() const { return udpListenAddress_; }
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

        void removeTckpSession(const TkcpSessionPtr& sess);
        void removeTckpSessionInLoop(const TkcpSessionPtr& sess);

        void onUdpMessage(UdpMessagePtr& msg);
        int outPutUdpMessage(const TkcpSessionPtr& sess, const char* buf, size_t len);

    private:

    typedef boost::unordered_map<uint32_t, TkcpSessionPtr> SessionMap;
    EventLoop* loop_;
    const InetAddress tcpListenAddress_;
    const InetAddress udpListenAddress_;
    const string name_;

    TkcpConnectionCallback tkcpConnectionCallback_;
    TkcpMessageCallback tkcpMessageCallback_;

    AtomicInt32 started_;

    uint32_t nextConv_;
    SessionMap sessions_;

    boost::scoped_ptr<UdpService> udpService_;
    TcpServer tcpserver_;
};


}

}
