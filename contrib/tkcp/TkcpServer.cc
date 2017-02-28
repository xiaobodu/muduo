
#include <boost/bind.hpp>

#include <muduo/base/Logging.h>

#include "TkcpServer.h"
#include "TkcpDefine.h"




namespace muduo {

namespace net {

TkcpServer::TkcpServer(EventLoop* loop,
                       const InetAddress& tcpListenAddr,
                       const InetAddress& udpListenAddr,
                       const string& nameArg)
    : loop_(CHECK_NOTNULL(loop)),
      tcpListenAddress_(tcpListenAddr),
      udpListenAddress_(udpListenAddr),
      name_(nameArg),
      tkcpConnectionCallback_(defaultTkcpConnectionCallback),
      tkcpMessageCallback_(defaultTkcpMessageCallback),
      nextConnId_(1),
      udpService_(loop, udpListenAddr),
      tcpserver_(loop, tcpListenAddr, nameArg+":Tkcp") {

    tcpserver_.setConnectionCallback(boost::bind(&TkcpServer::newTcpConnection, this, _1));
    udpService_.SetUdpMessageCallback(boost::bind(&TkcpServer::onUdpMessage, this, _1));

}

TkcpServer::~TkcpServer() {
}
void TkcpServer::Start() {
    if (started_.getAndSet(1) == 0) {
        tcpserver_.start();
        udpService_.Start();
    }
}

void TkcpServer::newTcpConnection(const TcpConnectionPtr& conn) {
    LOG_DEBUG << "tcp conn " << "from " << conn->peerAddress().toIpPort()
              << " " << (conn->connected() ? "UP" : "DOWN");
    if (conn->connected()) {

        Buffer buf;
        size_t dataLen = sizeof(uint16_t) + sizeof(int8_t) + udpListenAddress_.toIpPort().size();
        buf.appendInt16(static_cast<int16_t>(dataLen));
        buf.appendInt8(KTcpCmdConnectionSyncS2C);
        buf.append(udpListenAddress_.toIpPort().c_str(), udpListenAddress_.toIpPort().size());

        conn->send(&buf);
    }



}

void TkcpServer::removeTckpSession(const TkcpSessionPtr& sess) {
}

void TkcpServer::removeTckpSessionInLoop(const TkcpSessionPtr& sess) {
}

void TkcpServer::onUdpMessage(UdpMessagePtr& msg) {
    LOG_DEBUG << "new udp message from " << msg->intetAddress().toIpPort();
}

}
}
