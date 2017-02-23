
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/base/Logging.h>

#include "UdpSocket.h"



using namespace muduo;
using namespace muduo::net;

UdpSocket::UdpSocket()
    : sockfd_(createBlockingUDP(PF_INET)) {
    }
UdpSocket::~UdpSocket() {
    sockets::close(sockfd_);
}

int UdpSocket::createBlockingUDP(sa_family_t family) {
    int sockfd = ::socket(family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
    if (sockfd < 0) {
        LOG_SYSFATAL << "::socket";
    }
    return sockfd;
}

void UdpSocket::BindAddress(const InetAddress& localaddr) {
    sockets::bindOrDie(sockfd_, localaddr.getSockAddr());
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

void UdpSocket::SetTosWithLowDelay() {
    unsigned char lowDelay = IPTOS_LOWDELAY;
    int ret = ::setsockopt(sockfd_, IPPROTO_IP, IP_TOS, &lowDelay, sizeof(lowDelay));
    if (ret < 0) {
        LOG_SYSERR << "Set IPTOS_LOWDELAY failed.";
    }
}
