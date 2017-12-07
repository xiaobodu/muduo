#pragma once


#include <stdint.h>
#include <stddef.h>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/any.hpp>


#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/TimerId.h>

#include <kcp/ikcp.h>
#include "KcpCallback.h"




namespace muduo {

namespace net {
    typedef boost::function<int(const KcpConnectionPtr&, const char *, size_t)> UdpOutputCallback;
    class KcpConnection : public boost::noncopyable,
                        public boost::enable_shared_from_this<KcpConnection> {
        public:
        public:
            KcpConnection(uint32_t conv,
                        const InetAddress& localAddressForUdp,
                        const string& nameArg);
            ~KcpConnection();

            bool Connected() const { return state_ == kConnected; }
            bool Disconnected() const { return state_ == kDisconnected; }

            void Send(const void* message, int len);
            void Send(const StringPiece& message);
            void Send(Buffer* message);



            void SetUdpOutCallback(const UdpOutputCallback& cb) { udpOutputCallback_ = cb; }
            void SetTkcpCloseCallback(const TkcpCloseCallback& cb) { tkcpCloseCallback_ = cb; }
            void SetTkcpConnectionCallback(const TkcpConnectionCallback& cb) { tkcpConnectionCallback_ = cb; }
            void SetTkcpMessageCallback(const TkcpMessageCallback& cb) { tkcpMessageCallback_ = cb ; }

            uint32_t conv() const { return conv_; };
            const string name() const { return name_; }
            EventLoop* loop() const { return loop_; }

            void SetContext(const boost::any& context) { context_ = context; }
            const boost::any& GetContext() const { return context_; }
            boost::any* GetMutableContext() { return &context_; }



            const InetAddress& localAddress() { return localAddress_; }
            const InetAddress& remoteAddress() { return remoteAddress_; }

            void ConnectDestroyed();

            void Shutdown();

        private:


            void SendInLoop(const StringPiece& message);
            void SendInLoop(const void *message, size_t len);



        private:
            enum StateE {
                          kConnecting,
                          kConnected,
                          kDisconnecting,
                          kDisconnected,
            };
            const char* stateToString() const;

        private:
            int handleKcpOutput(const char* buf, int len);
            static int kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user);

            void setState(StateE s) { state_ = s; }

        private:
            EventLoop* loop_;
            uint32_t conv_;
            string name_;
            StateE state_;

            InetAddress remoteAddress_;
            InetAddress localAddress_;

            ikcpcb* kcpcb_;


            KcpCloseCallback closeCallback_;
            KcpConnectionCallback connectionCallback_;
            KcpMessageCallback messageCallback_;



            boost::any context_;
    };


    typedef boost::shared_ptr<KcpConnection> KcpConnectionPtr;

}

}