#include <contrib/tkcp/UdpSocket.h>
#include <contrib/tkcp/UdpMessage.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>

#include <unistd.h>

using namespace muduo;
using namespace muduo::net;


int main(void)
{
    Logger::setLogLevel(Logger::DEBUG);
    InetAddress v6Addr(5000, false, true);
    LOG_DEBUG << "V6: " << v6Addr.toIpPort();
    UdpSocket v6Socket(UdpSocket::IPV6);


    v6Socket.SetReuseAddr(true);
    v6Socket.SetReusePort(true);
    v6Socket.SetTosWithLowDelay();


    v6Socket.BindAddress(v6Addr);

    InetAddress v4Addr(5000);
    LOG_DEBUG << "V4: " << v4Addr.toPort();
    UdpSocket v4Socket(UdpSocket::IPV4);
    v4Socket.SetReuseAddr(true);
    v4Socket.SetReusePort(true);
    v4Socket.SetTosWithLowDelay();
    v4Socket.BindAddress(v4Addr);
    typedef boost::shared_ptr<UdpMessage> UdpMessagePtr;
    for (;;) {
        int ret = 0;
        UdpMessagePtr message;

        boost::tie(ret, message) = v4Socket.RecvMsg();

        if (ret >= 0) {
            v4Socket.SendMsg(message);
        }
    }

    return 0;
}
