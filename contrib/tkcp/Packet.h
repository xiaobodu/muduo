#pragma once

#include <stdint.h>
#include <stddef.h>

#include <muduo/base/Types.h>



namespace muduo {


namespace net {


class Buffer;

namespace packet {

namespace tcp {


const size_t kPacketHeadLength = 3;

struct TcpPacketHead {
    uint16_t len;
    uint8_t  packetId;
};


enum TcpPacketId {
    kUdpConnectionInfo = 1,
    kData = 2,
};


struct UdpConnectionInfo : public TcpPacketHead {
    uint32_t conv;
    string ip;
    uint16_t port;
    void Encode(Buffer* buf);
    void Decode(Buffer* buf);
    size_t Size();
};

string  PacketIdToString(uint8_t packetId);

} // namespace tcp


namespace udp {

const size_t kPacketHeadLength = 3;
struct UdpPacketHead {
    uint32_t conv;
    uint8_t  packetId;
};

enum UdpPacketId {
    kConnectSyn = 100,
    kConnectSynAck = 101,
    kConnectAck    = 102,
    kPingRequest   = 103,
    kPingReply     = 104,
    kData          = 105,
};

string PacketIdToString(uint8_t packetId);

} // namespace udp

} // namespace packet

} // namespace net

} // namespace muduo
