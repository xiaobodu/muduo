#include <cassert>


#include <muduo/base/Logging.h>

#include "Fec.h"
#include "Coding.h"
#include "TkcpDefine.h"
#include "Packet.h"

namespace muduo {
namespace net{

Fec::Fec(int redundant)
    : redundant_(redundant), 
      sendSeq_(0) {
    bzero(receivedSeqs_, sizeof(receivedSeqs_));
    sendBuffers_.resize(redundant_+1);

}

int Fec::Mtu() {
    if (redundant_ <= 0) {
        return UDP_MIN_MTU - packet::udp::kPacketHeadLength;
    } else {
        return (UDP_MIN_MTU - packet::udp::kPacketHeadLength - (redundant_+1)*FecHeadLen) / (redundant_+1);
    }
}
void Fec::Send(const char* data, size_t size) {
    assert(size <= static_cast<size_t>(Mtu()));

    if (redundant_ > 0) {
        sendBuffers_[0].retrieveAll();
        for (std::vector<Buffer>::size_type i = 0; i < sendBuffers_.size() - 1; ++i) {
            sendBuffers_[i].swap(sendBuffers_[i+1]);
        }
        EncodeUint16(&sendBuffers_[sendBuffers_.size()-1], sendSeq_++);
        EncodeUint16(&sendBuffers_[sendBuffers_.size()-1], static_cast<uint16_t>(size));
        sendBuffers_[sendBuffers_.size()-1].append(data, size);
        outBuffer_.retrieveAll();
        for (std::vector<Buffer>::size_type i = 0; i < sendBuffers_.size(); ++i) {
            outBuffer_.append(sendBuffers_[i].peek(), sendBuffers_[i].readableBytes());
        }
        sendOutCallback_(outBuffer_.peek(), outBuffer_.readableBytes());
    } else {
        sendOutCallback_(data, size);
    }

}

void Fec::Input(const char* data, size_t size) {
    if (redundant_ > 0) {
         inputBuf_.append(data, size);
         while (inputBuf_.readableBytes() > 0) {
             uint16_t seq = DecodeUint16(&inputBuf_);
             uint16_t len = DecodeUint16(&inputBuf_);
             uint16_t index = seq % ReceivedSeqsLen;
             if (seq != receivedSeqs_[index]) {
                 receivedSeqs_[index] = seq;
                 recvOutCallback_(inputBuf_.peek(), len);
             }
             inputBuf_.retrieve(len);
        }
        assert(inputBuf_.readableBytes() == 0);
    } else {
        recvOutCallback_(data, size);
    }
 
}
}
}
