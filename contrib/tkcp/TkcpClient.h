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
                   const string& nameArg);
        ~TkcpClient();

        void Connect();
        void Disconnect();

        EventLoop* getLoop() const { return loop_; }
        const string& name() const { return name_; }

        TkcpConnectionPtr conn() const { return kcpConn_; }

        void SetTkcpConnectionCallback(const TkcpConnectionCallback& cb) {
            tkcpConnectionCallback_ = cb;
        }

        void SetTkcpMessageCallback(const TkcpMessageCallback& cb) {
            tkcpMessageCallback_ = cb ;
        }



    public:
    private:
        void newTcpConnection(const TcpConnectionPtr& conn);
        void removeTckpconnion(const TkcpConnectionPtr& conn);

    private:

        void udpMessageCallback(const UdpClientSocketPtr& socket, Buffer* buf, Timestamp) ;
        void udpConnectCallback(const UdpClientSocketPtr& socket, UdpClientSocket::ConnectResult result) ;
        void udpDisconnectCallback(const UdpClientSocketPtr& socket, UdpClientSocket::DisconnectResutl result);


    private:
        muduo::net::EventLoop* loop_;
        const string name_;
        InetAddress peerAddress_;

        TkcpConnectionCallback tkcpConnectionCallback_;
        TkcpMessageCallback tkcpMessageCallback_;



        TcpClient tcpClient_;
        TkcpConnectionPtr kcpConn_;

        UdpClientSocketPtr socket_;
};


typedef boost::shared_ptr<TkcpClient> TkcpClientPtr;
}

}
