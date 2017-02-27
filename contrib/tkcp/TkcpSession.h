#pragma once


#include <stdint.h>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>


#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>

#include "kcp/ikcp.h"
#include "TkcpCallback.h"
#include "UdpMessage.h"



namespace muduo {

namespace net {
    typedef boost::function<int(const TkcpSessionPtr&, const char *, int)> UdpOutputCallback;
    class TkcpSession : public boost::noncopyable,
                        public boost::enable_shared_from_this<TkcpSession> {
        public:
        public:
            TkcpSession(EventLoop* loop, uint32_t conv, TcpConnectionPtr& tcpConnectionPtr);
            ~TkcpSession();

            EventLoop* GetLoop() const { return loop_; }
            bool Connected() const { return state_ == KConnected; }
            bool Disconnected() const { return state_ == KDisconnected; }




            void SetUdpOutCallback(const UdpOutputCallback& cb) { udpOutputCallback_ = cb; }
            void SetTkcpCloseCallback(TkcpCloseCallback& cb) { tkcpCloseCallback_ = cb; }
            void SetTkcpConnectionCallback(TkcpConnectionCallback& cb) { tkcpConnectionCallback_ = cb; }
            void SetTkcpMessageCallback(TkcpMessageCallback& cb) { tkcpMessageCallback_ = cb ; }

            uint32_t conv() const { return conv_; };

            const InetAddress& peerAddrForUdp() { return peerAddrForUdp_; }
            const InetAddress& peerAddrForTcp() { return tcpConnectionPtr_->peerAddress(); }

            void InUdpMessage(UdpMessagePtr& msg);

        private:
            enum StateE { KDisconnected, KConnecting, KConnected, kDisconnecting };

        private:
            int handleKcpout(const char* buf, int len);
            static int KcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user);

            void setState(StateE s) { state_ = s; }

            void onTcpConnection(const TcpConnectionPtr& conn);
            void onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);

        private:
            EventLoop* loop_;
            uint32_t conv_;

            TcpConnectionPtr tcpConnectionPtr_;
            StateE state_;

            InetAddress peerAddrForUdp_;
            ikcpcb* kcpcb_;


            TkcpCloseCallback tkcpCloseCallback_;
            TkcpConnectionCallback tkcpConnectionCallback_;
            TkcpMessageCallback tkcpMessageCallback_;


            UdpOutputCallback udpOutputCallback_;

    };

}

}







