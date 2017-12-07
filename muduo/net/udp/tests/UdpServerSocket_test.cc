

#include <iostream>

#include <muduo/net/InetAddress.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/inspect/Inspector.h>
#include <muduo/net/udp/UdpServerSocket.h>

using namespace muduo;
using namespace muduo::net;


int64_t g_BytesRead = 0;
int64_t g_MessageRead = 0;
int64_t g_BytesWritten = 0;
double g_time = 15;


void MyMessageCallback(UdpServerSocketPtr socket, Buffer* buf, Timestamp , const InetAddress& address) {
    ++g_MessageRead;
    g_BytesRead += buf->readableBytes();
    g_BytesWritten += buf->readableBytes();
    socket->SendTo(buf, address);
}

void stat() {
    std::cout << "BytesRead " << g_BytesRead << std::endl;
    std::cout << "BytesWritten " << g_BytesWritten << std::endl;
    std::cout <<  "MessageRead " << g_MessageRead << std::endl;
    std::cout << static_cast<double>(g_MessageRead) / g_time  << " PPS" << std::endl;
    std::cout << static_cast<double>(g_BytesRead) / (g_time*1024*1024)  << " MiB/s throughput" << std::endl;
    g_BytesRead = 0;
    g_BytesWritten = 0;
    g_MessageRead = 0;
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

        EventLoopThread t;
        Inspector ins(t.startLoop(), InetAddress(12345), "PingPong");

        auto socket = UdpServerSocket::MakeUdpServerSocket(&loop, listenAddr, "PingPong");

        socket->SetMessageCallback(MyMessageCallback);

        loop.runEvery(g_time, stat);
        socket->Start();
        loop.loop();
    }


    return 0;
}
