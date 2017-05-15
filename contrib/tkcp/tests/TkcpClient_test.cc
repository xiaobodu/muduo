
#include <stdio.h>

#include <muduo/base/Logging.h>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <vector>

#include <contrib/tkcp/TkcpClient.h>


using namespace muduo;
using namespace muduo::net;



void OnMessage(const TkcpSessionPtr& conn, Buffer* buffer) {
    string hello(buffer->peek(), buffer->readableBytes());
    LOG_DEBUG << "received " << hello;
    conn->Send(buffer);
    buffer->retrieveAll();
}

void sendMsg(const TkcpSessionPtr& conn) {
    char hello[] = "hello";
    conn->Send(hello, sizeof(hello));
}
void OnConnection(const TkcpSessionPtr& conn) {
    conn->GetLoop()->runEvery(1/30.0, boost::bind(sendMsg, conn));
}



int main(int argc, char* argv[])
{
    if (argc < 4) {
        printf("arg error usage: ip port n\n");
        return 1;
    }

    Logger::setLogLevel(Logger::DEBUG);

    std::vector<TkcpClientPtr> clients;
    EventLoop loop;
    InetAddress serverAddr(argv[1], static_cast<uint16_t>(atoi(argv[2])));
    int n = atoi(argv[3]);

    for (int i = 0; i < n; ++i) {
        TkcpClientPtr client(new TkcpClient(&loop, serverAddr, "test"));
        client->SetTkcpMessageCallback(OnMessage);
        client->SetTkcpConnectionCallback(OnConnection);
        client->Connect();
        clients.push_back(client);
    }

    loop.loop();

    return 0;
}
