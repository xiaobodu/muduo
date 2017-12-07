


#include <muduo/net/InetAddress.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <contrib/tkcp/UdpClient.h>
#include <iostream>

using namespace muduo;
using namespace muduo::net;


UdpClientPtr g_UdpClient;

int64_t g_BytesRead = 0;
int64_t g_BytesWritten = 0;
int64_t g_MessageRead = 0;
Timestamp start;
string g_Message;

void MyMessageCallback(Buffer* buf, Timestamp) {
    g_MessageRead++;
    g_BytesRead += buf->readableBytes();
    g_BytesWritten += buf->readableBytes();
    g_UdpClient->Write(buf);
}

void MyConnectCallback(UdpClient::ConnectResult result) {
    std::cout << "MyConnectCallback";
    if (result == UdpClient::kConnectSuccess) {
        start = Timestamp::now();
        g_UdpClient->Write(g_Message.data(), g_Message.size());
    }
}

void stop(EventLoop* loop) {
    std::cout << "BytesRead " << g_BytesRead << std::endl;
    std::cout << "BytesWritten " << g_BytesWritten << std::endl;
    std::cout <<  "MessageRead " << g_MessageRead << std::endl;
    auto now = Timestamp::now();
    std::cout << static_cast<double>(g_MessageRead) / timeDifference(now, start) << " PPS" << std::endl;
    std::cout << static_cast<double>(g_BytesRead) / (timeDifference(now, start) * 1024 * 1024) << " MiB/s throughput" << std::endl;
    loop->quit();

}


int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: server <address> <port> <blocksize> \n");
    } else {
        const char* ip = argv[1];
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        uint16_t blocksize = static_cast<uint16_t>(atoi(argv[3]));

        for (int i = 0; i < blocksize; ++i) {
            g_Message.push_back(static_cast<char>(i % 128));
        }
        InetAddress serverAddr(ip, port);

        Logger::setLogLevel(Logger::DEBUG);
        EventLoop loop;

        g_UdpClient = UdpClient::MakeUdpclient(&loop, "pingpong");

        g_UdpClient->SetMessageCallback(MyMessageCallback);
        g_UdpClient->SetConnectCallback(MyConnectCallback);


        g_UdpClient->Connect(serverAddr);
        loop.runAfter(60, boost::bind(stop, &loop));

        loop.loop();
    }


    return 0;
}
