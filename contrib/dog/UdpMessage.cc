#include <muduo/net/SocketsOps.h>

#include "UdpMessage.h"


using namespace muduo;
using namespace muduo::net;


UdpMessage::UdpMessage(const boost::shared_ptr<Buffer>& buffer, const InetAddress& intetAddress)
    : intetAddress_(intetAddress),
      buffer_(buffer){}


