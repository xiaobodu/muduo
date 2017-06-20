
#include <stdlib.h>

#include <contrib/rudp/ReliableUdpServer.h>
#include <contrib/rudp/cpu.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Atomic.h>


#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::rudp;


AtomicInt64 g_BytesRead;
AtomicInt64 g_MessageRead;
AtomicInt64 g_BytesWritten;
AtomicInt64 g_ConnNum;
double g_time = 15;


AtomicInt32 cpuCounter;

void ThreadInitcb(EventLoop* loop) {
    (void)loop;
    CoreAffinitize(cpuCounter.getAndAdd(1));
}

void OnConnection(const ReliableUdpConnectionPtr& conn) {
    if (conn->Connected()) {
        g_ConnNum.increment();
        LOG_INFO << "New Connetion";
    } else {
        g_ConnNum.decrement();
        LOG_INFO << "Connection close";
    }
}



void stat() {
    std::cout << "BytesRead " << g_BytesRead.get() << std::endl;
    std::cout << "BytesWritten " << g_BytesWritten.get() << std::endl;
    std::cout <<  "MessageRead " << g_MessageRead.get() << std::endl;
    std::cout << static_cast<double>(g_MessageRead.get()) / g_time  << " PPS" << std::endl;
    std::cout << static_cast<double>(g_BytesRead.get()) / (g_time*1024*1024)  << " MiB/s throughput" << std::endl;
    std::cout << "load num " << g_ConnNum.get() << std::endl;
    g_BytesRead.getAndSet(0);
    g_BytesWritten.getAndSet(0);
    g_MessageRead.getAndSet(0);
}



void OnMessage(const ReliableUdpConnectionPtr& conn, Buffer* buf) {
    g_MessageRead.increment();
    g_BytesRead.add(buf->readableBytes());
    g_BytesWritten.add( buf->readableBytes());
    int mid = static_cast<int>(buf->readableBytes()) / 2;
    conn->Send(buf->peek(), mid);
    buf->retrieve(mid);
    conn->Send(buf);
    buf->retrieveAll();
}


int main(int argc, char *argv[])
{

    muduo::Logger::setLogLevel(muduo::Logger::DEBUG);
    if (argc < 5) {
        fprintf(stderr, "Usage: sever address port serverId iothreadNUm \n");
        return 1;
    }
    uint16_t port = static_cast<uint16_t>(::atoi(argv[2]));
    InetAddress listenAddr(argv[1], port);
    uint32_t serverId = static_cast<uint32_t>(atoi(argv[3]));
    int ioThreadNum = atoi(argv[4]);

    EventLoop loop;


    CoreAffinitize(cpuCounter.getAndSet(1));

    boost::shared_ptr<ReliableUdpServer> server(new ReliableUdpServer(&loop,
                listenAddr,
                "test",
                serverId));

    server->setConnectionCallback(OnConnection);
    server->setMessageCallback(OnMessage);
    server->setThreadNum(ioThreadNum);
    server->setSocketNum(ioThreadNum);
    server->setThreadInitCallback(ThreadInitcb);
    server->Start();
    loop.runEvery(g_time, stat);


    loop.loop();


    return 0;
}



