#pragma once


#include <deque>
#include <vector>

#include "ReliableByteBuffer.h"
#include "ReliableDefine.h"
#include "ReliableFlusher.h"
#include "ReliableRingBuffer.h"
#include "ReliableRtoCalc.h"

namespace muduo {
namespace net {
namespace rudp {
// 可靠缓冲区
class ReliableBuffer
{
public:

	// maxWindowSize: 最大窗口大小
	ReliableBuffer(uint16_t maxWindowSize, uint16_t maxPacketLength);

	~ReliableBuffer();

	// 最大窗口大小
	uint16_t getMaxWindowSize() const;

	// 当前窗口大小, 只能增，不能减
	uint16_t getWindowSize() const;

	// 对端窗口大小
	uint16_t getPeerWindowSize() const;

	// 设置rto范围
	void setRtoBound(uint32_t minRto, uint32_t maxRto);

	// 设置延迟模式，ack包会延迟发送
	void setAckDelay(bool delay, uint32_t delayMills = DATAGRAM_DELAY_TIME);

	// 当前rtt
	uint32_t getNowRtt() const;

	// 平均rtt
	uint32_t getAverageRtt() const;

	// 当前rto
	uint32_t getNowRto() const;

	// 丢包率,单位0.001
	uint32_t getPackLossRate() const;

	// 包ping值
	uint32_t getPackPingValue() const;

	// 发送缓冲区已满
	bool isFullSendWindow() const;

	// 发送缓冲区已清空
	bool isEmptySendWindow() const;

	// 写入数据
	bool put(uint32_t nowts, const void* buffer, std::size_t len);

	// 读取数据
	std::size_t get(void* buffer, std::size_t len);

	// 头部数据
	bool top(const void*& buffer, std::size_t& len);

	// 弹出头部数据
	void pop();

	// 流输入
	void input(uint32_t nowts, const void* buffer, std::size_t len);

	// 是否是第一个包
	static bool isFirstRequestPacket(const void* buffer, uint16_t len);

	// 是否是第一个包
	static bool isFirstResponsePacket(const void* buffer, uint16_t len);

	// 清空缓冲区，返回下一次update的时间, quicMode 快速模式
	uint32_t update(uint32_t nowts, ReliableFlusher& flush, uint16_t redundant = 0);

	// 距离下一次update的时间 
	uint32_t check(uint32_t nowts) const;

private:

	std::size_t putSendBuffer(uint32_t nowts, const void* buffer, std::size_t len);

	// 移动putBuffer数据到发送buffer
	std::size_t fullSendBuffer(uint32_t nowts);

	// 重设重传时间
	void resetResentts();

	// 对端写入
	void push(uint32_t nowts, const DatagramHead& head, uint32_t sn, uint16_t len, const void* buffer);

	// 写入ack
	void putAck(uint32_t nowts, const DatagramHead& head, uint32_t sn);

	// 应答
	void ack(uint32_t nowts, const DatagramHead& head, uint32_t sn, uint32_t ts);

	// una
	void una(uint32_t nowts, const DatagramHead& head);

	// 解析数据指令
	bool parsePush(uint32_t nowts, const DatagramHead& head, const void* buffer, std::size_t len, std::size_t& index);

	// 解析应答指令
	bool parseAck(uint32_t nowts, const DatagramHead& head, const void* buffer, std::size_t len, std::size_t& index);

	// 统计丢包率
	void packLossRatePingValue(uint32_t nowTs, const DatagramSendData& data);
	
	// 最大窗口大小 
	uint16_t maxWindowSize_;
	// 接收窗口大小
	uint16_t recvWindowSize_;
	// 最早序号
	uint32_t recvEarliestSn_;
	// 接收窗口大小
	uint16_t sendWindowSize_;
	// 最早序号
	uint32_t sendEarliestSn_;
	// 当前序号
	uint32_t sendNextSn_;
	// 延迟模式
	bool ackDelayMode_;
	// 延迟时间
	uint32_t ackDelayTime_;
	// 最后生成ack的时间
	uint32_t ackNextTs_;
	// 接受缓冲区
	ReliableRingBuffer<DatagramRecvData> recvBuffer_;
	// 接受缓冲区顶部节点读取位置
	std::size_t recvReaderIndex_;
	// 应答队列
	ReliableRingBuffer<DatagramAck> ackQueue_;
	// 发送缓冲区
	ReliableRingBuffer<DatagramSendData> sendBuffer_;
	// 待发送数据
	ReliableByteBuffer putBuffer_;
	// 最大包大小
	uint16_t maxPacketLength_;
	// 冗余发送使用的临时缓冲区
	ReliableRingBuffer<std::size_t> redundants_;
	// rto计算
	ReliableRtoCalc rtoCalc_;
	// 5个包丢包率
	double packLossRate_;
	// 5个包ping值
	double packPingValue_;
	// 统计的包数
	uint32_t recordPackCount_;
};

}
}
}