#pragma once




#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>



#include <muduo/net/InetAddress.h>
#include "UdpMessage.h"





namespace muduo {

namespace net {
    class UdpSocket : public boost::noncopyable {
        public:
            enum  IPVersion {
                IPV4 = 0,
                IPV6 = 1,
            };
        public:
            explicit UdpSocket(IPVersion version, bool nonblock = false);
            ~UdpSocket();

            void BindAddress(const InetAddress& localaddr);
            int ConnectAddress(const InetAddress& peerAddress);

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
            static int createUDP(sa_family_t family, bool nonblock = false);
        private:
            const int sockfd_;
    };
}

}
