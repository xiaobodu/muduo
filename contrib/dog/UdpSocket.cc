
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

UdpSocket::UdpSocket(IPVersion version)
    : sockfd_(createBlockingUDP(version == IPV4 ? PF_INET : PF_INET6)) {
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
   socklen_t len = sizeof(timeout);
   int ret = setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
   if (ret < 0) {
       LOG_SYSERR << "SO_RCVTIMEO failed";
   }
}


void UdpSocket::SetTosWithLowDelay() {
    unsigned char lowDelay = IPTOS_LOWDELAY;
    int ret = ::setsockopt(sockfd_, IPPROTO_IP, IP_TOS, &lowDelay, sizeof(lowDelay));
    if (ret < 0) {
        LOG_SYSERR << "Set IPTOS_LOWDELAY failed.";
    }
}


boost::tuple<int, boost::shared_ptr<UdpMessage> > UdpSocket::RecvMsg() {
    const std::size_t initialSize = 1472;
    struct sockaddr fromAddr;
    ::bzero(&fromAddr, sizeof fromAddr);
    socklen_t addrLen = sizeof fromAddr;
    boost::shared_ptr<Buffer> recvBuffer = boost::make_shared<Buffer>(initialSize);
    ssize_t nr = ::recvfrom(sockfd_, recvBuffer->beginWrite(),
            recvBuffer->writableBytes(), 0, &fromAddr, &addrLen);

    if (nr >= 0) {
        recvBuffer->hasWritten(nr);
        InetAddress intetAddress;
        intetAddress.setSockAddrInet6(*sockets::sockaddr_in6_cast(&fromAddr));
        LOG_TRACE << "recvfrom return, readn = " << nr << " from " << intetAddress.toIpPort();
        return boost::make_tuple(0, boost::make_shared<UdpMessage>(recvBuffer, intetAddress));
    } else {
        LOG_SYSERR << "recvfrom return";
        return boost::make_tuple(-1, boost::shared_ptr<UdpMessage>());
    }
}


void UdpSocket::SendMsg(const boost::shared_ptr<UdpMessage>& msg) {
    ssize_t nw = ::sendto(sockfd_,
                      msg->buffer()->peek(),
                      msg->buffer()->readableBytes(),
                      0,
                      msg->intetAddress().getSockAddr(),
                      sizeof(struct sockaddr_in6));
    if (nw >= 0) {
        msg->buffer()->retrieve(nw);
    } else {
        LOG_SYSERR << "sendto failed";
    }
}
