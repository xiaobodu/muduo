

//#include <muduo/net/EventLoopThread.h>
//#include <muduo/net/inspect/Inspector.h>



#include <stdio.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>
#include <contrib/tkcp/TkcpServer.h>
#include <boost/bind.hpp>



using namespace muduo;
using namespace muduo::net;

void OnConnection(const TkcpConnectionPtr& sess) {
    LOG_INFO << "New TkcpConn ";
}

void OnMessage(const TkcpConnectionPtr& sess, Buffer* buffer) {
    LOG_DEBUG << buffer->toStringPiece().as_string();
    sess->Send(buffer);
}

void Stop(EventLoop* loop) {
    loop->quit();
}


int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("arg err Usage: ip port\n");
        return 1;
    }
    Logger::setLogLevel(Logger::INFO);
    InetAddress listenAddress(argv[1], static_cast<uint16_t>(atoi(argv[2])));
    //EventLoopThread t;
    //Inspector ins(t.startLoop(), InetAddress(12345), "PingPong");

    EventLoop loop;

    TkcpServer server(&loop, listenAddress, "test");
    server.SetTkcpConnectionCallback(OnConnection);
    server.SetTkcpMessageCallback(OnMessage);


    server.Start();

    loop.loop();

    return 0;
}
