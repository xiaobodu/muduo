#pragma once

#include <cstdint>
#include <stdlib.h>

#include <boost/unordered_set.hpp>
#include <boost/function.hpp>

#include <muduo/net/Buffer.h>

namespace  muduo {
namespace net {

typedef boost::function<void(const char*, size_t)> FecOutCallback;
class Fec {
    private:
        uint32_t sendSeq_;
        const static uint32_t ReceivedSeqsLen = 128;
        uint32_t receivedSeqs_[ReceivedSeqsLen];


        FecOutCallback sendOutCallback_;
        FecOutCallback recvOutCallback_;
        Buffer oneSendBuf_;
        Buffer secondSendBuf_;
        Buffer thirdSendBuf_;
        Buffer sendTotalBuf_;
        Buffer inputBuf_;
    private:

    public:
        const static size_t FecHeadLen = sizeof(uint32_t) + sizeof(uint16_t);
    public:
        Fec();
        void Send(const char* data, size_t size);
        void Input(const char* data, size_t size);
        void setSendOutCallback(const FecOutCallback& cb) { sendOutCallback_ = cb; }
        void setRecvOutCallback(const FecOutCallback& cb) { recvOutCallback_ = cb; }
};
}
}
