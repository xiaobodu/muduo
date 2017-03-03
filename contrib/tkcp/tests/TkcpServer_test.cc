
#include <stdio.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>
#include <contrib/tkcp/TkcpServer.h>

using namespace muduo;
using namespace muduo::net;


int main(int argc, char* argv[])
{
    if (argc < 4) {
        printf("arg err Usage: ip tcpport udpport\n");
        return 1;
    }

    Logger::setLogLevel(Logger::DEBUG);
    InetAddress tcpAddr(argv[1], static_cast<uint16_t>(atoi(argv[2])));
    InetAddress udpaddr(argv[1], static_cast<uint16_t>(atoi(argv[3])));


    EventLoop loop;

    TkcpServer server(&loop, tcpAddr, udpaddr, "test");

    server.Start();

    loop.loop();


    return 0;
}
