
#include "./ReliableDefine.h"
#include "./ReliableRingBuffer.h"
#include "./ReliableBuffer.h"


using namespace  muduo;
using namespace  muduo::net;
using namespace muduo::net::rudp;


ReliableRingBuffer<int> test() {

    return ReliableRingBuffer<int>(1024);
}