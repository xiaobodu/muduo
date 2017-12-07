#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <contrib/tkcp/TkcpClient.h>
#include <contrib/tkcp/TkcpServer.h>
#include <memory.h>
#include <stdio.h>



using namespace muduo;
using namespace muduo::net;



const size_t frameLen = 2*sizeof(int64_t);



void ServerMessageCallback(const TkcpConnectionPtr& sess,
                           Buffer* buffer) {

    int64_t message[2];

    memcpy(message, buffer->peek(), buffer->readableBytes());

    message[1] = Timestamp::now().microSecondsSinceEpoch();
    sess->Send(message, sizeof(message));
}


void runServer(const char* ip, uint16_t port) {
    EventLoop loop;
    TkcpServer server(&loop, InetAddress(ip, port), InetAddress(ip, static_cast<uint16_t>(port+1)), InetAddress(ip, static_cast<uint16_t>(port+1)), "clockSever");
    server.SetTkcpMessageCallback(ServerMessageCallback);
    server.Start();
    loop.loop();
}


TkcpConnectionPtr clientsess;

void ClientSessionCallback(const TkcpConnectionPtr& sess) {
    if (sess->Connected()) {
        clientsess = sess;
    } else {
        clientsess.reset();
    }
}


int64_t total = 0;
int64_t count = 0;
void ClientMessageCallback(const TkcpConnectionPtr&,
                           Buffer* buffer) {
    int64_t message[2];
    memcpy(message, buffer->peek(), buffer->readableBytes());

    int64_t send = message[0];
    int64_t their =  message[1];
    int64_t back = Timestamp::now().microSecondsSinceEpoch();
    int64_t mine = (back+send)/2;

    LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;
    total += back - send;
    ++count;

    LOG_INFO << "avg round trip " << total / count;

}


void sendMyTime() {
    if (clientsess) {
        int64_t message[2] = { 0, 0 };
        message[0] = Timestamp::now().microSecondsSinceEpoch();
        clientsess->Send(message, sizeof(message));
    }
}


void runClient(const char* ip, uint16_t port) {
    EventLoop loop;
    TkcpClient client(&loop, InetAddress(ip, port), "clockClient");
    client.SetTkcpConnectionCallback(ClientSessionCallback);
    client.SetTkcpMessageCallback(ClientMessageCallback);
    client.Connect();
    loop.runEvery(0.2, sendMyTime);
    loop.loop();
}


int main(int argc, char* argv[]) {
    if (argc > 2) {
        //Logger::setLogLevel(Logger::DEBUG);

        if (strcmp(argv[1], "-s") == 0) {
            uint16_t port = static_cast<uint16_t>(atoi(argv[3]));
            runServer(argv[2], port);
        } else {
            uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
            runClient(argv[1], port);
        }
    } else {

        printf("Usage:\n%s -s ip port\n%s ip port\n", argv[0], argv[0]);
    }
}

