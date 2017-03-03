
#include <stdio.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>
#include <contrib/tkcp/TkcpServer.h>

using namespace muduo;
using namespace muduo::net;

void OnConnection(const TkcpSessionPtr& sess) {
    LOG_DEBUG << "New TkcpConn";
}

void OnMessage(const TkcpSessionPtr& sess, Buffer* buffer) {
    sess->Send(buffer);
}


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
    server.SetTkcpConnectionCallback(OnConnection);
    server.SetTkcpMessageCallback(OnMessage);

    server.Start();

    loop.loop();


    return 0;
}
