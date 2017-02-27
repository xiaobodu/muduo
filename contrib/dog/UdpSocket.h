#pragma once




#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>



#include <muduo/net/InetAddress.h>
#include "UdpMessage.h"





namespace muduo {

namespace net {
    class UdpSocket : boost::noncopyable {
        public:
            enum  IPVersion {
                IPV4 = 0,
                IPV6 = 1,
            };
        public:
            explicit UdpSocket(IPVersion version);
            ~UdpSocket();

            void BindAddress(const InetAddress& localaddr);

            void SetReuseAddr(bool on);

            void SetReusePort(bool on);

            void SetTosWithLowDelay();

            void SetRecvTimeout(int millisecond);

            void SetSendTimeout(int millisecond);

            void SetSendBuf(int bytes);
            void SetRecvBuf(int bytes);

            int sockfd() const { return sockfd_; }

            boost::tuple<int, boost::shared_ptr<UdpMessage> > RecvMsg();
            void SendMsg(const boost::shared_ptr<UdpMessage>& msg);

        private:
            static int createBlockingUDP(sa_family_t family);
        private:
            const int sockfd_;
    };
}

}
