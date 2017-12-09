
#include <stdio.h>

#include <muduo/base/Logging.h>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <vector>

#include <contrib/tkcp/TkcpClient.h>
#include <muduo/base/CurrentThread.h>


using namespace muduo;
using namespace muduo::net;

string g_Message;

void OnMessage(const TkcpConnectionPtr& conn, Buffer* buffer) {
    LOG_DEBUG << buffer->toStringPiece().as_string();
    buffer->retrieveAll();
}

void sendMsg(const TkcpConnectionPtr& conn) {
    conn->Send(g_Message);
}
void OnConnection(const TkcpConnectionPtr& conn) {
    conn->getLoop()->runEvery(1/30.0, boost::bind(sendMsg, conn));
}



int main(int argc, char* argv[])
{
    if (argc < 5) {
        printf("arg error usage: ip port clientnum packetsize\n");
        return 1;
    }

    Logger::setLogLevel(Logger::INFO);

    std::vector<TkcpClientPtr> clients;
    EventLoop loop;
    InetAddress serverAddr(argv[1], static_cast<uint16_t>(atoi(argv[2])));
    int clientnum  = atoi(argv[3]);
    int packetsize = atoi(argv[4]);

    for (int i = 0; i < packetsize; ++i) {
        g_Message.push_back(static_cast<char>(i % 128));
    }

    for (int i = 0; i < clientnum; ++i) {
        TkcpClientPtr client(new TkcpClient(&loop, serverAddr, "test"));
        client->SetMessageCallback(OnMessage);
        client->SetConnectionCallback(OnConnection);
        client->Connect();
        CurrentThread::sleepUsec(12*1000); 
        clients.push_back(client);
    }

    loop.loop();

    return 0;
}
