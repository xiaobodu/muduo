#include <muduo/net/SocketsOps.h>

#include "UdpMessage.h"


using namespace muduo;
using namespace muduo::net;


UdpMessage::UdpMessage(const boost::shared_ptr<Buffer>& buf, const InetAddress& intetAddr)
    : intetAddress_(intetAddr),
      buffer_(buf){}


