#pragma once


#include <boost/noncopyable.hpp>


#include <muduo/net/InetAddress.h>





namespace muduo {

namespace net {
    class UdpSocket : boost::noncopyable {
        public:
            explicit UdpSocket();
            ~UdpSocket();

            void BindAddress(const InetAddress& localaddr);

            void SetReuseAddr(bool on);

            void SetReusePort(bool on);

            void SetTosWithLowDelay();

            int sockfd() const { return sockfd_; }

        private:
            static int createBlockingUDP(sa_family_t family);
        private:
            const int sockfd_;
    };
}

}
