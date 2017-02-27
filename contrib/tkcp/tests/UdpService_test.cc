#include <contrib/tkcp/UdpService.h>
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>


using namespace muduo;
using namespace  muduo::net;

void MyMessageCallback(UdpService* udpService, UdpMessagePtr msg) {
    udpService->SendMsg(msg);
}

void stop(UdpService* udpService, EventLoop* loop) {
    udpService->Stop();
    loop->quit();
}

int main(void)
{
   // Logger::setLogLevel(Logger::TRACE);
    EventLoop loop;
    InetAddress v4Addr(5000);

    UdpService udpService(&loop, v4Addr);

    udpService.SetUdpMessageCallback(boost::bind(MyMessageCallback, &udpService, _1));
    udpService.Start();

    loop.runAfter(70, boost::bind(stop, &udpService, &loop));

    loop.loop();

    return 0;
}
