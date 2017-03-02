#include <muduo/net/SocketsOps.h>

#include "UdpMessage.h"


using namespace muduo;
using namespace muduo::net;


UdpMessage::UdpMessage(const boost::shared_ptr<Buffer>& buf, const InetAddress& inetAddr)
    : intetAddress_(inetAddr),
      buffer_(buf){}

UdpMessage::UdpMessage(const char* buf, size_t len, const InetAddress& inetAddr)
    : intetAddress_(inetAddr),
      buffer_(new Buffer(len)){
    buffer_->append(buf, len);
}
