
#include <stdint.h>


#include <muduo/base/Types.h>
#include <muduo/net/Buffer.h>
#include <stdio.h>

#include "Coding.h"
#include "Packet.h"


namespace muduo {


namespace net {


namespace packet {

namespace tcp {


void UdpConnectionInfo::Encode(Buffer* buf) {
    EncodeUint16(buf, static_cast<uint16_t>(Size()));
    EncodeUint8(buf, packetId);
    EncodeUint32(buf, conv);
}

void UdpConnectionInfo::Decode(Buffer* buf) {
    len = DecodeUint16(buf);
    packetId = DecodeUint8(buf);
    conv = DecodeUint32(buf);
}

size_t UdpConnectionInfo::Size() {
    return kPacketHeadLength + sizeof(conv);
}

string  PacketIdToString(uint8_t packetId) {
    switch(packetId) {
        case kUdpConnectionInfo:
            return "kUdpConnectionInfo";
        case kData:
            return "kData";
        case kPingRequest:
            return "KPingRequest";
        case kPingReply:
            return "kPingReply";
        default:
            {
                char buf[64] = {0};
                ::snprintf(buf, sizeof(buf), "unknown tcp packet id %u", packetId);
                return buf;
            }
    }
}

} // namespace tcp


namespace udp {

string PacketIdToString(uint8_t packetId) {
    switch(packetId) {
        case kConnectSyn:
            return "kConnectSyn";
        case kConnectSynAck:
            return "kConnectSynAck";
        case kPingRequest:
            return "kPingRequest";
        case kPingReply:
            return "kPingReply";
        case kData:
            return "kData";
        default:
            {
                char buf[64] = {0};
                ::snprintf(buf, sizeof(buf), "unknown udp packet id %u", packetId);
                return buf;
            }
    }
}

} // namespace udp

} // namespace packet

} // namespace net

} // namespace muduo
