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
    uint16_t len; // 包含包头长度
    uint8_t  packetId;
};


enum TcpPacketId {
    kUdpConnectionInfo = 1,
    kData = 2,
    KUseTcp = 3,
    kPingRequest = 4,
    kPingReply = 5,
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

const size_t kPacketHeadLength = 5;
struct UdpPacketHead {
    uint32_t conv;
    uint8_t  packetId;
};

enum UdpPacketId {
    kConnectSyn = 100,
    kConnectSynAck = 101,
    kPingRequest   = 102,
    kPingReply     = 103,
    kData          = 104,
};

string PacketIdToString(uint8_t packetId);

} // namespace udp

} // namespace packet

} // namespace net

} // namespace muduo
