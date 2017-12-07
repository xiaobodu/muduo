

#include <iostream>

#include <muduo/net/InetAddress.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <contrib/udp/UdpServerSocket.h>

using namespace muduo;
using namespace muduo::net;




void MyMessageCallback(UdpServerSocketPtr socket, Buffer* buf, Timestamp , const InetAddress& address) {
    socket->SendTo(buf, address);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: server <address> <port> \n");
    } else {
        const char* ip = argv[1];
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        InetAddress listenAddr(ip, port);

        Logger::setLogLevel(Logger::DEBUG);
        EventLoop loop;


        auto socket = UdpServerSocket::MakeUdpServerSocket(&loop, listenAddr, "PingPong");

        socket->SetMessageCallback(MyMessageCallback);

        socket->Start();
        loop.loop();
    }


    return 0;
}
