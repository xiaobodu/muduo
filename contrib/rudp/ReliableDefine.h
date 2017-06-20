#pragma once

#include <endian.h>

#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <limits>
#include <vector>


namespace muduo {
namespace net {
namespace rudp {

#if  __GNUC_PREREQ (4,6)
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
// ipv4的mss
// (mtu 576) - (ip head 20 ) - (udp head 8)
const uint16_t DATAGRAM_MSS_V4 = 548;

// ipv6的mtu
// (mtu 1280) - (ip head 40 ) - (udp head 8)
const uint16_t DATAGRAM_MSS_V6 = 1232;

// 8个字节的会话长度
const uint16_t DATAGRAM_CONV_SIZEOF = 8;

// crc长度
const uint16_t DATAGAM_CRC_SIZEOF = 2;

// 握手消息长度, sizeof(uint64_t)
const uint16_t DATAGAM_HANDSHAKE_SIZEOF = 8;

// 包头长度
const uint16_t DATAGRAM_HEAD_SIZEOF = 10;

// 命令长度
const uint16_t DATAGRAM_CMD_SIZEOF = 1;

// 同步命令长度
const uint16_t DATAGRAM_SYNC_SIZEOF = 4;

// 结束命令长度
const uint16_t DATAGRAM_FIN_SIZEOF = 4;

// 应答包长
const uint16_t DATAGRAM_ACK_SIZEOF = 8;

// 数据包包头长度
const uint16_t DATAGRAM_DATA_HEAD_SIZEOF = 6;

// ipv4最大数据包长度
// (mss 548) - (conv id 8) - (checksum 2) - (head 10) - (cmd 1) - (pack head 6)
const uint16_t DATAGRAM_PACK_SIZE_V4 = 521;

// ipv6最大数据包长度
// (mss 1232) - (conv id 8) - sizeof(checksum 2) - sizeof(head 10) - sizeof(cmd 1) -szieof(pack head 6)
const uint16_t DATAGRAM_PACK_SIZE_V6 = 1205;

// 数据包大小
const uint16_t DATAGRAM_PACK_SIZE = DATAGRAM_PACK_SIZE_V6;

// v4的数据buffer大小
// (mss 548) - sizeof(conv id 8) - sizeof(checksum 2)
const uint16_t DATAGRAM_BUFFER_SIZE_V4 = 538;

// v6的数据buffer大小
// (mss 1232) - sizeof(conv id 8) - sizeof(checksum 2)
const uint16_t DATAGRAM_BUFFER_SIZE_V6 = 1222;

// 连接建立命令
const uint8_t DATAGRAM_CMD_SYN = 1;

// 数据发送命令
const uint8_t DATAGRAM_CMD_PSH = 2;

// 数据应答命令
const uint8_t DATAGRAM_CMD_ACK = 3;

// 连接断开命令
const uint8_t DATAGRAM_CMD_FIN = 4;

//最小重传超时时间
const uint32_t DATAGRAM_RTO_MIN = 20;

//最大重传超时时间
const uint32_t DATAGRAM_RTO_MAX = 3000;

//默认重传超时时间
const uint32_t DATAGRAM_RTO_DEFAULT = 200;

// 默认的延迟时间
const uint32_t DATAGRAM_DELAY_TIME = 20;

// 包头
struct DatagramHead
{
	//时间戳
	uint32_t ts;
	// 最后应答序号
	uint32_t una;
	// 本端窗口大小
	uint16_t wnd;
};

// 数据包命令
struct DatagramCmd
{
	uint8_t cmd;
};

// 同步
struct DatagramSync
{
	uint32_t sn;
};

// 结束
struct DatagramFinsh
{};

// 包应答
struct DatagramAck
{
	//应答序号
	uint32_t sn;
	//应答时间戳
	uint32_t ts;
};

// 包数据
struct DatagramRecvData
{
	// 数据序号
	uint32_t sn;
	//数据长度
	uint16_t len;
	// 数据包是否有效
	bool available;
	// 数据
	std::vector<char> data;
};

// 重传数据
struct DatagramSendData
{
	// 数据序号
	uint32_t sn;
	//数据长度
	uint16_t len;
	// 发送时间
	uint32_t sentts;
	// 发送次数
	uint32_t resent;
	// 下一次重传时间
	uint32_t resendts;
	// 最近rto
	uint32_t rto;
	// 是否应答
	bool ack;
	// 数据
	std::vector<char> data;
};

// 获取保存的最早的序号
inline uint32_t calcEarliestSequenceNo(uint32_t nextSequenceNo, uint16_t diff)
{
	const uint64_t maxUint32 = std::numeric_limits<uint32_t>::max();
	return (nextSequenceNo + maxUint32 - diff) % maxUint32;
}

// 下一个序号
inline uint32_t calcNextSequenceNo(uint32_t sn, uint32_t diff = 1)
{
	return (static_cast<uint64_t>(sn) + diff) % std::numeric_limits<uint32_t>::max();
}

// 序号之间的差值
inline uint32_t calcDiffSequenceNo(uint32_t startSequenceNo, uint32_t stopSequenceNo)
{
	const uint64_t maxUint32 = std::numeric_limits<uint32_t>::max();
	return (stopSequenceNo + maxUint32 - startSequenceNo) % maxUint32;
}


// 序号顺序
inline bool needAckSequenceNo(uint32_t recvNextSn, uint32_t sn, uint16_t windowSize)
{
	const uint32_t maxUint32 = std::numeric_limits<uint32_t>::max();
	if (recvNextSn < windowSize)
	{
		return sn <= recvNextSn || sn > maxUint32 / 2;
	}
	if (recvNextSn >= windowSize && sn < maxUint32 - windowSize)
	{
		return sn <= recvNextSn;
	}
	else
	{
		return sn <= recvNextSn || sn > maxUint32 / 2;
	}
}

// 序列化
template <typename T>
inline std::size_t Serialize(const T& element, void* buffer, std::size_t index)
{
	std::memcpy(static_cast<char*>(buffer) + index, &element, sizeof(element));
	index += sizeof(element);
	return index;
}

#define DATAGRAMSERIALIZE(n) \
inline std::size_t DatagramSerialize(const uint##n##_t element, void* buffer, std::size_t index) { \
    uint##n##_t le##n = htole##n(element); \
    return Serialize(le##n, buffer, index); \
}

DATAGRAMSERIALIZE(16)
DATAGRAMSERIALIZE(32)
DATAGRAMSERIALIZE(64)

inline std::size_t DatagramSerialize(const uint8_t element, void* buffer, std::size_t index) {
    return Serialize(element, buffer, index);
}

inline std::size_t DatagramSerialize(const void* data, std::size_t size, void* buffer, std::size_t index)
{
	std::memcpy(static_cast<char*>(buffer) + index, data, size);
	index += size;
	return index;
}

// 反序列化
template <typename T>
inline std::size_t Deserialize(T& element, const void* buffer, std::size_t index)
{
	std::memcpy(&element, static_cast<const char*>(buffer) + index, sizeof(element));
	index += sizeof(element);
	return index;
}

inline std::size_t DatagramDeserialize(uint8_t& element, const void* buffer, std::size_t index) {
    return Deserialize(element, buffer, index);
}

#define DATAGRAMDESERIALIZE(n) \
inline std::size_t DatagramDeserialize(uint##n##_t& element, const void* buffer, std::size_t index) { \
   index = Deserialize(element, buffer, index); \
   element = le##n##toh(element);\
   return index; \
}

DATAGRAMDESERIALIZE(16)
DATAGRAMDESERIALIZE(32)
DATAGRAMDESERIALIZE(64)

// 缓冲区
typedef std::vector<std::pair<const void*, std::size_t> > ReliableDataBuffers;

#if  __GNUC_PREREQ (4,6)
#pragma GCC diagnostic pop
#else
#pragma GCC diagnostic warning "-Wconversion"
#pragma GCC diagnostic warning "-Wold-style-cast"
#endif

}
}
}
