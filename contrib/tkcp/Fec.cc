#include <cassert>


#include <muduo/base/Logging.h>

#include "Fec.h"
#include "Coding.h"


namespace muduo {
namespace net{

Fec::Fec()
    : sendSeq_(0){}

void Fec::Send(const char* data, size_t size) {
    assert(size <= 175);
    oneSendBuf_.retrieveAll();
    oneSendBuf_.swap(secondSendBuf_);
    secondSendBuf_.swap(thirdSendBuf_);

    EncodeUint32(&thirdSendBuf_, sendSeq_++);
    EncodeUint16(&thirdSendBuf_, static_cast<uint16_t>(size));
    thirdSendBuf_.append(data, size);

    sendTotalBuf_.append(oneSendBuf_.peek(), oneSendBuf_.readableBytes());
    sendTotalBuf_.append(secondSendBuf_.peek(), secondSendBuf_.readableBytes());
    sendTotalBuf_.append(thirdSendBuf_.peek(), thirdSendBuf_.readableBytes());
    sendOutCallback_(sendTotalBuf_.peek(), sendTotalBuf_.readableBytes());
    sendTotalBuf_.retrieveAll();
}

void Fec::Input(const char* data, size_t size) {
    inputBuf_.append(data, size);

    while (inputBuf_.readableBytes() > 0) {
        uint32_t seq = DecodeUint32(&inputBuf_);
        uint16_t len = DecodeUint16(&inputBuf_);
        auto it = receivedSeqSet_.find(seq);
        if (it == receivedSeqSet_.end()) {
            receivedSeqSet_.insert(seq);
            recvOutCallback_(inputBuf_.peek(), len);
        }
        inputBuf_.retrieve(len);
    }
    assert(inputBuf_.readableBytes() == 0);

}
}
}
