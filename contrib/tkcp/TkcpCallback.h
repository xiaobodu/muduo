#pragma once

#include <boost/function.hpp>
#include <muduo/base/Timestamp.h>

namespace muduo {



namespace net {

class Buffer;
class TkcpConnection;

typedef boost::shared_ptr<TkcpConnection> TkcpConnectionPtr;
typedef boost::function<void (const TkcpConnectionPtr&)> TkcpConnectionCallback;
typedef boost::function<void (const TkcpConnectionPtr&)> TkcpCloseCallback;
typedef boost::function<void (const TkcpConnectionPtr&, Buffer*)> TkcpMessageCallback;


void defaultTkcpConnectionCallback(const TkcpConnectionPtr& conn);
void defaultTkcpMessageCallback(const TkcpConnectionPtr& conn,
                                Buffer* buffer);

}

}
