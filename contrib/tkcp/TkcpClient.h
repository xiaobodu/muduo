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
#include <muduo/net/Channel.h>


#include "TkcpSession.h"
#include "TkcpCallback.h"



namespace muduo {

namespace net {
class UdpSocket;

class TkcpClient : public boost::noncopyable {
    public:
        TkcpClient(EventLoop* loop,
                   const InetAddress& serverAddrForTcp,
                   const string& nameArg);
        ~TkcpClient();

        void Connect();
        void Disconnect();

        EventLoop* getLoop() const { return loop_; }
        const string& name() const { return name_; }

        TkcpSessionPtr Sess() const { return sess_; }

        void SetTkcpConnectionCallback(const TkcpConnectionCallback& cb) {
            tkcpConnectionCallback_ = cb;
        }

        void SetTkcpMessageCallback(const TkcpMessageCallback& cb) {
            tkcpMessageCallback_ = cb ;
        }



    public:
    private:
        void newTcpConnection(const TcpConnectionPtr& conn);
        void removeTckpSession(const TkcpSessionPtr& sess);

    private:
        // for udp begin
        void handleRead(Timestamp receiveTime);
        void handWrite();
        void handError();
        void connectUdpsocket();
        int sendUdpMsg(const char* buf, size_t len);
        int outPutUdpMessage(const TkcpSessionPtr& sess, const char* buf, size_t len);

        // for udp end

    private:
        muduo::net::EventLoop* loop_;
        const string name_;
        InetAddress serverAddrForTcp_;
        InetAddress serverAddrForUdp_;

        TkcpConnectionCallback tkcpConnectionCallback_;
        TkcpMessageCallback tkcpMessageCallback_;

        bool udpsocketConnected;
        boost::scoped_ptr<Channel> udpChannel_;
        boost::scoped_ptr<UdpSocket> udpsocket_;

        TcpClient tcpClient_;
        TkcpSessionPtr sess_;

        typedef std::queue<string> stringqueue;
        stringqueue outputUdMessagesCache_;
};


typedef boost::shared_ptr<TkcpClient> TkcpClientPtr;
}

}
