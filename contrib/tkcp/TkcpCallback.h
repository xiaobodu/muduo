#pragma once

#include <boost/function.hpp>
#include <boost/function.hpp>
#include <muduo/base/Timestamp.h>

namespace muduo {



namespace net {

class Buffer;
class TkcpSession;

typedef boost::shared_ptr<TkcpSession> TkcpSessionPtr;
typedef boost::function<void (const TkcpSessionPtr&)> TkcpConnectionCallback;
typedef boost::function<void (const TkcpSessionPtr&)> TkcpCloseCallback;
typedef boost::function<void (const TkcpSessionPtr&, Buffer*)> TkcpMessageCallback;


void defaultTkcpConnectionCallback(const TkcpSessionPtr& conn);
void defaultTkcpMessageCallback(const TkcpSessionPtr& conn,
                                Buffer* buffer);

}

}
