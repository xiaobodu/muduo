#include <muduo/net/SocketsOps.h>

#include "UdpMessage.h"


using namespace muduo;
using namespace muduo::net;


UdpMessage::UdpMessage(std::size_t defaultBufferSize)
    : buffer_(defaultBufferSize) {}

string UdpMessage::toIpPort() const {
    char buf[64] = "";
    sockets::toIpPort(buf, sizeof buf, sockets::sockaddr_cast(&fromAddr_));
    return buf;
}
