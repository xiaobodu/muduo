
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
    EncodeUint16(buf, static_cast<uint16_t>(ip.size()));
    buf->append(ip.c_str(), ip.size());
    EncodeUint16(buf, port);
}

void UdpConnectionInfo::Decode(Buffer* buf) {
    len = DecodeUint16(buf);
    packetId = DecodeUint8(buf);
    conv = DecodeUint32(buf);
    size_t ipSize = static_cast<size_t>(DecodeUint16(buf));
    ip = string(buf->peek(), ipSize);
    buf->retrieve(ipSize);
    port = DecodeUint16(buf);
}

size_t UdpConnectionInfo::Size() {
    return kPacketHeadLength + sizeof(conv) + sizeof(uint16_t) +
        ip.size() + sizeof(port);
}

string  PacketIdToString(uint8_t packetId) {
    switch(packetId) {
        case kUdpConnectionInfo:
            return "kUdpConnectionInfo";
        case kData:
            return "kData";
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
        case kConnectAck:
            return "kConnectAck";
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
