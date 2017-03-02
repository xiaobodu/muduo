
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <strings.h>


#include <boost/make_shared.hpp>
#include <muduo/net/SocketsOps.h>
#include <muduo/base/Logging.h>

#include "UdpSocket.h"



using namespace muduo;
using namespace muduo::net;

UdpSocket::UdpSocket(IPVersion version, bool nonblock)
    : sockfd_(createUDP(version == IPV4 ? PF_INET : PF_INET6, nonblock)) {
    }
UdpSocket::~UdpSocket() {
    sockets::close(sockfd_);
}

int UdpSocket::createUDP(sa_family_t family, bool nonblock) {
    int type = SOCK_DGRAM | SOCK_CLOEXEC;
    if (nonblock) {
        type = type | SOCK_NONBLOCK;
    }

    int sockfd = ::socket(family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
    if (sockfd < 0) {
        LOG_SYSFATAL << "::socket";
    }
    return sockfd;
}

void UdpSocket::BindAddress(const InetAddress& localaddr) {
    sockets::bindOrDie(sockfd_, localaddr.getSockAddr());
}

int UdpSocket::ConnectAddress(const InetAddress& peerAddress) {
    return sockets::connect(sockfd_, peerAddress.getSockAddr());
}


void UdpSocket::SetReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval,
            static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        LOG_SYSERR << "SO_REUSEADDR failed.";
    }
}


void UdpSocket::SetReusePort(bool on) {
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
            &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on)
    {
        LOG_SYSERR << "SO_REUSEPORT failed.";
    }
#else
    if (on)
    {
        LOG_ERROR << "SO_REUSEPORT is not supported.";
    }
#endif
}

void UdpSocket::SetRecvTimeout(int millisecond) {
    struct timeval timeout;
    timeout.tv_sec = millisecond / 1000;
    timeout.tv_usec = (millisecond % 1000) * 1000;
    socklen_t len = sizeof(timeout);

    int ret = setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
    if (ret < 0) {
        LOG_SYSERR << "SO_RCVTIMEO failed";
    }
}

void UdpSocket::SetSendTimeout(int millisecond) {
   struct timeval timeout;
   timeout.tv_sec = millisecond / 1000;
   timeout.tv_usec = (millisecond % 1000) * 1000;
   socklen_t len = sizeof timeout;
   int ret = setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
   if (ret < 0) {
       LOG_SYSERR << "SO_RCVTIMEO failed";
   }
}

void UdpSocket::SetRecvBuf(int bytes) {
    socklen_t len = sizeof bytes;

    int ret = setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, &bytes, len);
    if (ret < 0) {
        LOG_SYSERR << "SO_RCVBUF failed";
    }
}

void UdpSocket::SetSendBuf(int bytes) {
    socklen_t len = sizeof bytes;

    int ret = setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &bytes, len);

    if (ret < 0) {
        LOG_SYSERR << "SO_SNDBUF failed";
    }
}


void UdpSocket::SetTosWithLowDelay() {
    unsigned char lowDelay = IPTOS_LOWDELAY;
    int ret = ::setsockopt(sockfd_, IPPROTO_IP, IP_TOS, &lowDelay, sizeof(lowDelay));
    if (ret < 0) {
        LOG_SYSERR << "Set IPTOS_LOWDELAY failed.";
    }
}



