#pragma once

#include <boost/function.hpp>
#include <boost/function.hpp>
#include <muduo/base/Timestamp.h>

namespace muduo {



namespace net {

class Buffer;
class KcpConnection;

typedef boost::shared_ptr<KcpConnection> KcpConnectionPtr;
typedef boost::function<void (const KcpConnectionPtr&)> KcpConnectionCallback;
typedef boost::function<void (const KcpConnectionPtr&)> KcpCloseCallback;
typedef boost::function<void (const KcpConnectionPtr&, Buffer*)> KcpMessageCallback;


void defaultKcpConnectionCallback(const KcpConnectionPtr& conn);
void defaultKcpConnectionMessageCallback(const KcpConnectionPtr& conn,
                                Buffer* buffer);

}

}
