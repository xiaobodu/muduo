
#include <stdio.h>

#include <muduo/base/Logging.h>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <contrib/tkcp/TkcpClient.h>


using namespace muduo;
using namespace muduo::net;



int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("arg error usage: ip port\n");
        return 1;
    }

    Logger::setLogLevel(Logger::DEBUG);

    EventLoop loop;
    InetAddress serverAddr(argv[1], static_cast<uint16_t>(atoi(argv[2])));

    boost::shared_ptr<TkcpClient> client = boost::make_shared<TkcpClient>(&loop, serverAddr, "test");

    client->Connect();
    loop.loop();

    return 0;
}
