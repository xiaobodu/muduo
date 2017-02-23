#include <contrib/dog/UdpSocket.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>


using namespace muduo;
using namespace muduo::net;


int main(void)
{
    Logger::setLogLevel(Logger::DEBUG);
    InetAddress addr(5000);
    LOG_DEBUG << addr.toIpPort();
    UdpSocket socket;

    socket.BindAddress(addr);

    socket.SetReuseAddr(true);
    socket.SetReusePort(true);
    socket.SetTosWithLowDelay();

    return 0;
}
