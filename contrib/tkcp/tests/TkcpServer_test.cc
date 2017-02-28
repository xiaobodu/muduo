

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>
#include <contrib/tkcp/TkcpServer.h>

using namespace muduo;
using namespace muduo::net;


int main(void)
{
    Logger::setLogLevel(Logger::DEBUG);
    InetAddress tcpAddr(5001);
    InetAddress udpaddr(5000);


    EventLoop loop;

    TkcpServer server(&loop, tcpAddr, udpaddr, "test");

    server.Start();

    loop.loop();


    return 0;
}
