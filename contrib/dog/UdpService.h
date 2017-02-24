#pragma once


#include <sys/socket.h>

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>


#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>
#include <muduo/base/BlockingQueue.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Socket.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/Channel.h>


#include "UdpMessage.h"
#include "UdpSocket.h"






namespace muduo {

namespace net {
    class UdpService;

    typedef boost::shared_ptr<UdpMessage> UdpMessagePtr;
    typedef boost::shared_ptr<UdpService> UdpServicePtr;
    typedef boost::function<void(UdpServicePtr, UdpMessagePtr, Timestamp)> UdpMessageCallback;

    class UdpService : public boost::noncopyable,
                       public boost::enable_shared_from_this<UdpService> {
        public:
            UdpService(const InetAddress& localaddr);
            ~UdpService();
            void SetUdpMessageCallback(UdpMessageCallback cb) { udpMessageCallback_ = cb; }
            void SendMsg(UdpMessagePtr& msg);

            void Start();
            void Stop();

        private:
            void stopInRecvLoop();

            void SendMsgInSendLoop(UdpMessagePtr& msg);


            void recvLoopThreadInitCallback(EventLoop* loop);
            void sendLoopThreadInitCallback(EventLoop* loop);

            void handleRead(Timestamp receiveTime);
            void handleWrite();
            void handleError();


        private:
            string name_;
            boost::shared_ptr<EventLoopThread> recvLoopThread_;
            EventLoop* recvLoop_;
            boost::shared_ptr<EventLoopThread> sendLoopThread_;
            EventLoop* sendLoop_;
            UdpSocket udpSocket_;

            boost::scoped_ptr<Channel> channel_;

            UdpMessageCallback udpMessageCallback_;

            bool started_;

    };
}

}
