#pragma once


#include <stdint.h>
#include <stddef.h>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/any.hpp>


#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/TimerId.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>


#include "TkcpCallback.h"



struct IKCPCB;
namespace muduo {

namespace net {
    class Fec;
    typedef boost::function<int(const TkcpConnectionPtr&, const char *, size_t)> UdpOutputCallback;
    class TkcpConnection : public boost::noncopyable,
                        public boost::enable_shared_from_this<TkcpConnection> {
        public:
        public:
            TkcpConnection(uint32_t conv,
                        const InetAddress& localUdpAddress,
                        const InetAddress& peerUdpAddress,
                        const TcpConnectionPtr& tcpConnectionPtr,
                        const string& nameArg, const int redundant_ = 0);
            ~TkcpConnection();

            EventLoop* getLoop() const { return loop_; }
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
            void SetContext(const boost::any& context) { context_ = context; }
            const boost::any& GetContext() const { return context_; }
            boost::any* GetMutableContext() { return &context_; }


            const InetAddress& localTcpAddress() { return tcpConnectionPtr_->localAddress(); }
            const InetAddress& localUdpAddress() { return localUdpAddress_; }
            const InetAddress& peerUdpAddress() { return peerUdpAddress_; }
            const InetAddress& peerTcpAddress() { return tcpConnectionPtr_->peerAddress(); }

            void InputUdpMessage(Buffer* buf, const InetAddress& peerAddress);

            void SyncUdpConnectionInfo();
            void onTcpConnection(const TcpConnectionPtr& conn);
            void onTcpMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);
            void ConnectDestroyed();

            void Shutdown();
            void Close();

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
            void onUpdateKcp();
            void immediatelyUpdateKcp();
            void immediatelyUpdateKcp(uint32_t current);
            void postImmediatelyUpdateKcp();
            void updateKcp(uint32_t current);

            void sendKcpMsg(const void *data, size_t len);
            void onUdpData(const char* buf, size_t len);
            void onFecRecvData(const char* data, size_t len);
            void onFecSendData(const char* data, size_t len);

            void sendConnectSyn();
            void udpPingRequest();
            //for udp end

            //for tcp begin
            void onUdpconnectionInfo(Buffer* buf);
            void onTcpData(Buffer* buf);
            void onUseTcp(Buffer* buf);
            void tcpPingRequest();
            void onTcpPingRequest(Buffer* buf);
            void onTcpPingReply(Buffer* buf);
            void sendTcpMsg(const void *data, size_t len);
            //for tcp end


            void forceClose();

        private:
            enum StateE {
                          kTcpConnected,
                          kUdpConnectSynSend,
                          kConnected,
                          kDisconnecting,
                          kDisconnected,
            };

            enum KcpStateE {
                kUpdating = 0x1,
                kPosting  = 0x2,
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
            int redundant_;

            TcpConnectionPtr tcpConnectionPtr_;
            StateE state_;

            InetAddress peerUdpAddress_;
            InetAddress localUdpAddress_;
            bool kcpInited_;
            struct IKCPCB* kcpcb_;
            Buffer kcpRecvBuf_;
            boost::shared_ptr<Fec> fec_;
            Buffer udpSendBuf_;
            UdpOutputCallback udpOutputCallback_;


            TkcpCloseCallback tkcpCloseCallback_;
            TkcpConnectionCallback tkcpConnectionCallback_;
            TkcpMessageCallback tkcpMessageCallback_;



            TimerId connectSyncAckTimer_;
            int trySendConnectSynTimes;
            bool udpAvailble_;

            uint32_t kcpState_;
            TimerId updateKcpTimer_;
            uint32_t nextKcpTime_;


            TimerId udpPingTimer_;

            TimerId tcpPingTimer_;

            Timestamp lastRecvTcpPingDataTime_;
            Timestamp lastRecvUdpPingDataTime_;


            boost::any context_;
    };


    typedef boost::shared_ptr<TkcpConnection> TkcpConnectionPtr;

}

}







