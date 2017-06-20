#pragma once



#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <muduo/base/Timestamp.h>


namespace muduo {
namespace net {

class Buffer;

namespace rudp {


class ReliableUdpConnection;
typedef boost::shared_ptr<ReliableUdpConnection> ReliableUdpConnectionPtr;
typedef boost::function<void (const ReliableUdpConnectionPtr&)> ConnectionCallback;
typedef boost::function<void (const ReliableUdpConnectionPtr&)> CloseCallback;
typedef boost::function<void (const ReliableUdpConnectionPtr&, bool)> EstablishCallback;
typedef boost::function<void (const ReliableUdpConnectionPtr&,
                              Buffer*)> MessageCallback;

void DefaultConnectionCallback(const ReliableUdpConnectionPtr&);
void DefaultMessageCallback(const ReliableUdpConnectionPtr&,
                            Buffer* buffer);
}
}
}
