#include <muduo/net/InetAddress.h>
#include <contrib/dog/UdpService.h>
#include <muduo/base/Logging.h>
#include <boost/bind.hpp>
#include <muduo/base/Atomic.h>


using namespace muduo;
using namespace muduo::net;

AtomicInt64 count;
Timestamp countTime = Timestamp::now();


void MyMessageCallback(UdpServicePtr udpService, UdpMessagePtr msg, Timestamp receiveTime) {
    count.increment();
    udpService->SendMsg(msg);
}

void Count() {
    Timestamp now = Timestamp::now();
    LOG_INFO << static_cast<double>(count.get())/(muduo::timeDifference(now, countTime));
    count.getAndSet(0);
}



int main(void)
{
    Logger::setLogLevel(Logger::INFO);
    InetAddress addr(5000);

    EventLoop loop;
    boost::shared_ptr<UdpService> udpService(new UdpService(addr));
    udpService->SetUdpMessageCallback(MyMessageCallback);

    udpService->Start();
    loop.runEvery(30, Count);

    loop.loop();

    return 0;
}
