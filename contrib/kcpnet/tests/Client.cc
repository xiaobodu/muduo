


#include <muduo/net/InetAddress.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/udp/UdpClientSocket.h>
#include <iostream>

using namespace muduo;
using namespace muduo::net;





void MyMessageCallback(const UdpClientSocketPtr& socket, Buffer* buf, Timestamp) {
    LOG_DEBUG << buf->retrieveAllAsString();
}

void MyConnectCallback(const UdpClientSocketPtr& socket, UdpClientSocket::ConnectResult result) {
    std::cout << "MyConnectCallback";
    if (result == UdpClientSocket::kConnectSuccess) {
        string hello = "Hello World!";
        socket->Write(hello.c_str(), hello.size());
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: server <address> <port>  \n");
    } else {
        const char* ip = argv[1];
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));

        InetAddress serverAddr(ip, port);

        Logger::setLogLevel(Logger::DEBUG);
        EventLoop loop;

        auto socket = UdpClientSocket::MakeUdpClientSocket(&loop, "pingpong");

        socket->SetMessageCallback(MyMessageCallback);
        socket->SetConnectCallback(MyConnectCallback);


        socket->Connect(serverAddr);

        loop.loop();
    }


    return 0;
}
