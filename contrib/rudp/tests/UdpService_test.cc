#include <contrib/tkcp/UdpService.h>
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <stdio.h>


using namespace muduo;
using namespace  muduo::net;

void MyMessageCallback(UdpService* udpService, UdpMessagePtr msg) {
    udpService->SendMsg(msg);
}

void stop(UdpService* udpService, EventLoop* loop) {
    udpService->Stop();
    loop->quit();
}

int main(int argc, char* argv[])
{

    if (argc < 3) {
        printf("arg error usage: ip port\n");
    }
   // Logger::setLogLevel(Logger::TRACE);
    EventLoop loop;
    InetAddress v4Addr(argv[1], static_cast<uint16_t>(atoi(argv[2])));

    UdpService udpService(&loop, v4Addr);

    udpService.SetUdpMessageCallback(boost::bind(MyMessageCallback, &udpService, _1));
    udpService.Start();

    loop.runAfter(70, boost::bind(stop, &udpService, &loop));

    loop.loop();

    return 0;
}
