#pragma once


#include <stdint.h>
#include <stddef.h>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>


#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/TimerId.h>

#include "kcp/ikcp.h"
#include "TkcpCallback.h"
#include "UdpMessage.h"



namespace muduo {

namespace net {
    typedef boost::function<int(const TkcpSessionPtr&, const char *, size_t)> UdpOutputCallback;
    class TkcpSession : public boost::noncopyable,
                        public boost::enable_shared_from_this<TkcpSession> {
        public:
        public:
            TkcpSession(uint32_t conv,
                        const InetAddress& localAddressForUdp,
                        const TcpConnectionPtr& tcpConnectionPtr);
            ~TkcpSession();

            EventLoop* GetLoop() const { return loop_; }
            bool Connected() const { return state_ == KConnected; }
            bool Disconnected() const { return state_ == KDisconnected; }

            void Send(const void* message, int len);
            void Send(const StringPiece& message);
            void Send(Buffer* message);



            void SetUdpOutCallback(const UdpOutputCallback& cb) { udpOutputCallback_ = cb; }
            void SetTkcpCloseCallback(const TkcpCloseCallback& cb) { tkcpCloseCallback_ = cb; }
            void SetTkcpConnectionCallback(const TkcpConnectionCallback& cb) { tkcpConnectionCallback_ = cb; }
            void SetTkcpMessageCallback(const TkcpMessageCallback& cb) { tkcpMessageCallback_ = cb ; }

            uint32_t conv() const { return conv_; };


            const InetAddress& localAddressForTcp() { return tcpConnectionPtr_->localAddress(); }
            const InetAddress& LocalAddressForUdp() { return localAddressForUdp_; }
            const InetAddress& peerAddressForUdp() { return peerAddressForUdp_; }
            const InetAddress& peerAddressForTcp() { return tcpConnectionPtr_->peerAddress(); }

            void InputUdpMessage(UdpMessagePtr& msg);

            void SyncUdpConnectionInfo();
            void onTcpConnection(const TcpConnectionPtr& conn);
            void onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);
            void ConnectDestroyed();

        private:


            void SendInLoop(const StringPiece& message);
            void SendInLoop(const void *message, size_t len);
            //for udp begin
            void onConnectSyn();
            void onConnectSyncAck();
            void onConnectAck();
            void onPingRequest();
            void onPingReply();
            void initKcp();
            void kcpUpdate();
            void SendKcpMsg(const void *data, size_t len);
            void onUdpData(const char* buf, size_t len);
            //for udp end

            //for tcp begin
            void onUdpconnectionInfo(Buffer* buf);
            void onTcpData(Buffer* buf);
            //for tcp end

        private:
            enum StateE { KDisconnected, KConnecting, KConnected, kDisconnecting };

        private:
            int handleKcpOutput(const char* buf, int len);
            static int kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void *user);

            void setState(StateE s) { state_ = s; }

        private:
            EventLoop* loop_;
            uint32_t conv_;

            TcpConnectionPtr tcpConnectionPtr_;
            StateE state_;

            InetAddress peerAddressForUdp_;
            InetAddress localAddressForUdp_;
            ikcpcb* kcpcb_;


            TkcpCloseCallback tkcpCloseCallback_;
            TkcpConnectionCallback tkcpConnectionCallback_;
            TkcpMessageCallback tkcpMessageCallback_;


            UdpOutputCallback udpOutputCallback_;
            TimerId kcpUpdateTimer;
    };

}

}







