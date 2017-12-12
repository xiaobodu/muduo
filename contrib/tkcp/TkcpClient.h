#pragma once



#include <stddef.h>
#include <queue>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <muduo/base/Types.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>

#include <muduo/net/udp/UdpClientSocket.h>

#include "TkcpConnection.h"
#include "TkcpCallback.h"




namespace muduo {

namespace net {





class TkcpClient : public boost::noncopyable {
    public:
        TkcpClient(EventLoop* loop,
                   const InetAddress& peerAddress,
                   const string& nameArg,
                   const int redundant = 0);
        ~TkcpClient();

        void Connect();
        void Disconnect();

        EventLoop* getLoop() const { return loop_; }
        const string& name() const { return name_; }

        TkcpConnectionPtr conn() const { return conn_; }

        void SetConnectionCallback(const TkcpConnectionCallback& cb) {
            connectionCallback_ = cb;
        }

        void SetMessageCallback(const TkcpMessageCallback& cb) {
            messageCallback_ = cb ;
        }



    public:
    private:
        void newTcpConnection(const TcpConnectionPtr& conn);
        void removeTckpconnection(const TkcpConnectionPtr& conn);
        int outputUdpMessage(const TkcpConnectionPtr& conn, const char* buf, size_t len);

    private:

        void onUdpMessage(const UdpClientSocketPtr& socket, Buffer* buf, Timestamp) ;
        void onUdpConnect(const UdpClientSocketPtr& socket, UdpClientSocket::ConnectResult result) ;
        void onUdpDisconnect(const UdpClientSocketPtr& socket, UdpClientSocket::DisconnectResutl result);

    private:
        muduo::net::EventLoop* loop_;
        const string name_;
        InetAddress peerAddress_;
        int redundant_;

        TkcpConnectionCallback connectionCallback_;
        TkcpMessageCallback messageCallback_;



        TcpClient tcpClient_;
        TkcpConnectionPtr conn_;

        UdpClientSocketPtr socket_;
};


typedef boost::shared_ptr<TkcpClient> TkcpClientPtr;
}

}
