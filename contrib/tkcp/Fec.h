#pragma once

#include <cstdint>
#include <stdlib.h>

#include <boost/unordered_set.hpp>
#include <boost/function.hpp>
#include <vector>

#include <muduo/net/Buffer.h>

namespace  muduo {
namespace net {

typedef boost::function<void(const char*, size_t)> FecOutCallback;
class Fec {
    private:
        int redundant_;
        uint16_t sendSeq_;
        const static uint32_t ReceivedSeqsLen = 256;
        uint16_t receivedSeqs_[ReceivedSeqsLen];


        FecOutCallback sendOutCallback_;
        FecOutCallback recvOutCallback_;
        Buffer inputBuf_;

        std::vector<Buffer> sendBuffers_;
        Buffer outBuffer_;
    private:

    public:
        const static int FecHeadLen = sizeof(uint16_t) + sizeof(uint16_t);
    public:
        Fec(int redundant = 0);
        int Mtu();
        void Send(const char* data, size_t size);
        void Input(const char* data, size_t size);
        void setSendOutCallback(const FecOutCallback& cb) { sendOutCallback_ = cb; }
        void setRecvOutCallback(const FecOutCallback& cb) { recvOutCallback_ = cb; }
};
}
}
