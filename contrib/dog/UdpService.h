#pragma once


#include <sys/socket.h>

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/function.hpp>


#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>
#include <muduo/base/BlockingQueue.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Socket.h>
#include <muduo/net/EventLoop.h>


#include "UdpMessage.h"
#include "UdpSocket.h"






namespace muduo {

namespace net {

    typedef boost::shared_ptr<UdpMessage> UdpMessagePtr;
    typedef boost::function<void(UdpMessagePtr)> UdpMessageCallback;

    class UdpService : public boost::noncopyable {
        public:
            UdpService(EventLoop* loop, const InetAddress& localaddr);
            ~UdpService();
            EventLoop* getLoop() const { return loop_; }
            void SetUdpMessageCallback(UdpMessageCallback cb) { udpMessageCallback_ = cb; }
            void SendMsg(UdpMessagePtr& msg);

            void Start();
            void Stop();

        private:
            void runInUdpMsgRecvThread();
            void runInUdpMsgSendThread();
            void stopInLoop();


        private:
            EventLoop* loop_;
            UdpMessageCallback udpMessageCallback_;

            boost::shared_ptr<muduo::Thread> udpMsgRecvThread_;
            boost::shared_ptr<muduo::Thread> udpMsgSendThread_;


            BlockingQueue<UdpMessagePtr> udpMsgSendQueue_;

            UdpSocket udpSocket_;

            bool running_;
    };
}

}
