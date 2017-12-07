#include "ReliableBuffer.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <muduo/base/Logging.h>
using namespace muduo;

using namespace muduo::net::rudp;

ReliableBuffer::ReliableBuffer(uint16_t maxWindowSize, uint16_t maxPacketLength)
	: maxWindowSize_(maxWindowSize),
	recvWindowSize_(std::max<uint16_t>(maxWindowSize / 4, 1)),
	recvEarliestSn_(0),
	sendWindowSize_(std::max<uint16_t>(maxWindowSize / 4, 1)),
	sendEarliestSn_(0),
	sendNextSn_(0),
	ackDelayMode_(false),
	ackDelayTime_(DATAGRAM_DELAY_TIME),
	ackNextTs_(0),
	recvBuffer_(maxWindowSize_),
	recvReaderIndex_(0),
	ackQueue_(maxWindowSize_),
	sendBuffer_(maxWindowSize_),
	maxPacketLength_(maxPacketLength),
	redundants_(1),
	rtoCalc_(DATAGRAM_RTO_MIN, DATAGRAM_RTO_MAX, DATAGRAM_RTO_DEFAULT),
	packLossRate_(0),
	packPingValue_(0),
	recordPackCount_(0)
{
	assert(maxPacketLength > 0 && maxPacketLength <= DATAGRAM_PACK_SIZE);
	// 初始化接收数据buffer
	while (!recvBuffer_.full())
	{
		DatagramRecvData& data = recvBuffer_.push();
		data.data.resize(maxPacketLength_);
	}
	recvBuffer_.clear();
	// 初始化发送数据buffer
	while (!sendBuffer_.full())
	{
		DatagramSendData& data = sendBuffer_.push();
		data.data.resize(maxPacketLength_);
	}
	sendBuffer_.clear();
}

ReliableBuffer::~ReliableBuffer()
{}

uint16_t ReliableBuffer::getMaxWindowSize() const
{
	return maxWindowSize_;
}

uint16_t ReliableBuffer::getWindowSize() const
{
	return sendWindowSize_;
}

uint16_t ReliableBuffer::getPeerWindowSize() const
{
	return recvWindowSize_;
}

void ReliableBuffer::setRtoBound(uint32_t minRto, uint32_t maxRto)
{
	rtoCalc_.setRtoBound(minRto, maxRto);
}

void ReliableBuffer::setAckDelay(bool delay, uint32_t delayMills)
{
	ackDelayMode_ = delay;
	ackDelayTime_ = delayMills;
}

uint32_t ReliableBuffer::getNowRtt() const
{
	return rtoCalc_.rtt();
}

uint32_t ReliableBuffer::getNowRto() const
{
	return rtoCalc_.rto();
}

uint32_t ReliableBuffer::getAverageRtt() const
{
	return rtoCalc_.averageRtt();
}

uint32_t ReliableBuffer::getPackLossRate() const
{
	return static_cast<uint32_t>(packLossRate_ * 1000);
}

uint32_t ReliableBuffer::getPackPingValue() const
{
	return static_cast<uint32_t>(packPingValue_);
}

bool ReliableBuffer::isFullSendWindow() const
{
	return sendBuffer_.size() == sendWindowSize_;
}

bool ReliableBuffer::isEmptySendWindow() const
{
	return sendBuffer_.empty();
}

bool ReliableBuffer::put(uint32_t nowts, const void* buffer, std::size_t len)
{
	std::size_t putBytes = 0;
	if (putBuffer_.size() == 0)
	{
		putBytes = putSendBuffer(nowts, buffer, len);
		putBuffer_.put(static_cast<const char*>(buffer) + putBytes, len - putBytes);
	}
	else
	{
		putBuffer_.put(buffer, len);
		putBytes = fullSendBuffer(nowts);
	}
	return putBytes != 0;
}

std::size_t ReliableBuffer::get(void* buffer, std::size_t len)
{
	assert(buffer != 0);
	std::size_t index = 0;
	std::size_t nbytes = 0;
	while (!recvBuffer_.empty() && recvBuffer_.top().available && index < len)
	{
		DatagramRecvData& data = recvBuffer_.top();
		assert(data.sn == recvEarliestSn_);
		std::size_t n = std::min<std::size_t>(data.len - recvReaderIndex_, len - index);
		std::memcpy(static_cast<char*>(buffer) + index, data.data.data() + recvReaderIndex_, n);
		recvReaderIndex_ += n;
		index += n;
		nbytes += n;
		if (recvReaderIndex_ == data.len)
		{
			recvBuffer_.pop();
			recvReaderIndex_ = 0;
			recvEarliestSn_ = calcNextSequenceNo(recvEarliestSn_);
		}
	}
	return nbytes;
}

bool ReliableBuffer::top(const void*& buffer, std::size_t& len)
{
	if (!recvBuffer_.empty() && recvBuffer_.top().available)
	{
		DatagramRecvData& data = recvBuffer_.top();
		assert(data.sn == recvEarliestSn_);
		buffer = data.data.data() + recvReaderIndex_;
		len = data.len - recvReaderIndex_;
		return true;
	}
	return false;
}

void ReliableBuffer::pop()
{
	if (!recvBuffer_.empty() && recvBuffer_.top().available)
	{
		DatagramRecvData& data = recvBuffer_.top();
		assert(data.sn == recvEarliestSn_);
        (void)data;//去除在release模式下的警告
		recvBuffer_.pop();
		recvReaderIndex_ = 0;
		recvEarliestSn_ = calcNextSequenceNo(recvEarliestSn_);
	}
}

void ReliableBuffer::input(uint32_t nowts, const void* buffer, std::size_t len)
{
	std::size_t index = 0;
	// 数据包太短
	if (len < DATAGRAM_HEAD_SIZEOF)
	{
		LOG_DEBUG << "data too small";
		return;
	}
	// 保存rto
	uint32_t savedRto = rtoCalc_.rto();
	// 解析包头
	DatagramHead head;
	index = DatagramDeserialize(head.ts, buffer, index);
	index = DatagramDeserialize(head.una, buffer, index);
	index = DatagramDeserialize(head.wnd, buffer, index);
	// 对端窗口增加
	if (head.wnd > recvWindowSize_ && recvWindowSize_ < maxWindowSize_)
	{
		recvWindowSize_ = std::min(std::max(recvWindowSize_, head.wnd), maxWindowSize_);
	}
	// 更新una
	una(nowts, head);
	// 解析命令
	while (index < len)
	{
		uint8_t cmd;
		index = DatagramDeserialize(cmd, buffer, index);
		// 是否退出循环
		bool breakWhile = false;
		// 处理命令
		switch (cmd)
		{
		case DATAGRAM_CMD_PSH:
			if (!parsePush(nowts, head, buffer, len, index))
			{
				breakWhile = true;
			}
			break;
		case DATAGRAM_CMD_ACK:
			if (!parseAck(nowts, head, buffer, len, index))
			{
				breakWhile = true;
			}
			break;
		default:
			break;
		}
		if (breakWhile)
		{
			break;
		}
	}
	// 更新了rto, 重设超时时间
	if (rtoCalc_.rto() < savedRto)
	{
		resetResentts();
	}
}

bool ReliableBuffer::isFirstRequestPacket(const void* buffer, uint16_t len)
{
	std::size_t index = 0;
	// 数据包太短
	if (len < DATAGRAM_HEAD_SIZEOF)
	{
		return false;
	}
	// 解析包头
	DatagramHead head;
	index = DatagramDeserialize(head.ts, buffer, index);
	index = DatagramDeserialize(head.una, buffer, index);
	index = DatagramDeserialize(head.wnd, buffer, index);
	if (head.wnd == 0)
	{
		return false;
	}
	// 解析指令
	uint8_t cmd;
	if (index + sizeof(cmd) > len)
	{
		return false;
	}
	index = DatagramDeserialize(cmd, buffer, index);
	// 数据包
	if (cmd == DATAGRAM_CMD_PSH)
	{
		// 解析数据头
		if (index + DATAGRAM_DATA_HEAD_SIZEOF > len)
		{
			return false;
		}
		uint32_t sn = 0;
		uint16_t dataLen = 0;
		index = DatagramDeserialize(sn, buffer, index);
		index = DatagramDeserialize(dataLen, buffer, index);
		if (sn != 0 || index + dataLen > len)
		{
			return false;
		}
		return true;
	}
	return false;
}

bool  ReliableBuffer::isFirstResponsePacket(const void* buffer, uint16_t len)
{
	std::size_t index = 0;
	// 数据包太短
	if (len < DATAGRAM_HEAD_SIZEOF)
	{
		return false;
	}
	// 解析包头
	DatagramHead head;
	index = DatagramDeserialize(head.ts, buffer, index);
	index = DatagramDeserialize(head.una, buffer, index);
	index = DatagramDeserialize(head.wnd, buffer, index);
	if (head.wnd == 0)
	{
		return false;
	}
	// 解析指令
	uint8_t cmd;
	if (index + sizeof(cmd) > len)
	{
		return false;
	}
	index = DatagramDeserialize(cmd, buffer, index);
	if (cmd == DATAGRAM_CMD_ACK)
	{
		if (index + DATAGRAM_ACK_SIZEOF > len)
		{
			return false;
		}
		uint32_t sn = 0;
		index = DatagramDeserialize(sn, buffer, index);
		if (sn != 0)
		{
			return false;
		}
		return true;
	}
	return false;
}

// 清空缓冲区，返回下一次update的时间
uint32_t ReliableBuffer::update(uint32_t nowts, ReliableFlusher& flush, uint16_t redundant)
{
	// 下一次update时间
	uint32_t nextTs = nowts + rtoCalc_.maxRto();
	//组成头部
	DatagramHead head;
	head.ts = nowts;
	head.una = recvEarliestSn_;
	head.wnd = sendWindowSize_;
	flush.start(head);
	// 标记需要发送
	bool needFlush = false;
	// 输出应答
	if (!ackQueue_.empty() && ackNextTs_ <= nowts)
	{
		while (ackQueue_.size() > 1)
		{
			const DatagramAck& ack = ackQueue_.top();
			if (ack.sn > recvEarliestSn_)
			{
				flush.push(head, ack);
				needFlush = true;
			}
			ackQueue_.pop();
		}
		if (!ackQueue_.empty())
		{
			const DatagramAck& ack = ackQueue_.top();
			flush.push(head, ack);
			needFlush = true;
			ackQueue_.pop();
		}
		ackNextTs_ = 0;
	}
	else if (!ackQueue_.empty())
	{
		nextTs = ackNextTs_;
	}
	// 输出数据
	std::size_t flushNewDataNum = 0;
	redundants_.reserve(redundant);
	for (std::size_t i = 0; i != sendBuffer_.size(); ++i)
	{
		DatagramSendData& data = sendBuffer_[i];
		if (!data.ack)
		{
			if (data.resendts <= nowts)
			{
				flush.push(head, data);
				needFlush = true;
				data.rto = rtoCalc_.nextRto(data.rto);
				data.resent += 1;
				data.resendts = nowts + data.rto;
				flushNewDataNum += (data.resent == 1) ? 1 : 0;
				nextTs = std::min(nextTs, data.resendts);
			}
			else if (redundant != 0)
			{
				if (redundants_.size() == redundant)
				{
					nextTs = std::min(nextTs, sendBuffer_[redundants_.top()].resendts);
					redundants_.pop();
				}
				redundants_.push(i);
			}
			else
			{
				nextTs = std::min(nextTs, data.resendts);
			}
		}
	}
	// 发送冗余包,只有在最后一个包还能塞下时发送
	while (!redundants_.empty() && flushNewDataNum > 0)
	{
		std::size_t dataIndex = redundants_.top();
		redundants_.pop();
		DatagramSendData& data = sendBuffer_[dataIndex];
		if (flush.ensure(DATAGRAM_DATA_HEAD_SIZEOF + data.len))
		{
			flush.push(head, data);
			needFlush = true;
			data.rto = rtoCalc_.nextRto(data.rto);
			data.resent += 1;
			data.resendts = nowts + data.rto;
			nextTs = std::min(nextTs, data.resendts);
		}
		else
		{
			break;
		}
	}
	redundants_.clear();
	// 刷新
	if (needFlush)
	{
		flush.end();
	}
	// 还有数据可发送，可立即update一次
	if (sendBuffer_.size() < sendWindowSize_ && putBuffer_.size() > 0)
	{
		fullSendBuffer(nowts);
		nextTs = nowts;
	}
	nextTs = std::max(nextTs, nowts);
	return nextTs;
}

// 距离下一次update的时间
uint32_t ReliableBuffer::check(uint32_t nowts) const
{
	uint32_t nextTs = nowts + rtoCalc_.maxRto();
	if (!ackQueue_.empty())
	{
		nextTs = std::min(ackNextTs_, nextTs);
	}
	for (std::size_t i = 0; i != sendBuffer_.size(); ++i)
	{
		const DatagramSendData& data = sendBuffer_[i];
		if (!data.ack)
		{
			nextTs = std::min(data.resendts, nextTs);
		}
	}
	nextTs = std::max(nextTs, nowts);
	return nextTs;
}

std::size_t ReliableBuffer::putSendBuffer(uint32_t nowts, const void* buffer, std::size_t len)
{
	assert(buffer != 0);
	std::size_t index = 0;
	// 先看最后的节点是否还能够写入
	if (!sendBuffer_.empty() && index < len)
	{
		DatagramSendData& data = sendBuffer_.tail();
		if (data.resent == 0 && data.len < maxPacketLength_)
		{
			data.sentts = nowts;
			std::size_t n = std::min<std::size_t>(maxPacketLength_ - data.len, len - index);
			std::memcpy(data.data.data() + data.len, static_cast<const char*>(buffer) + index, n);
			index += n;
			data.len = static_cast<uint16_t>(data.len + n);
		}
	}
	// 写入到新的节点
	while (sendBuffer_.size() < sendWindowSize_ && index < len)
	{
		DatagramSendData& data = sendBuffer_.push();
		data.sn = sendNextSn_;
		sendNextSn_ = calcNextSequenceNo(sendNextSn_);
		data.len = std::min<uint16_t>(maxPacketLength_, static_cast<uint16_t>(len - index));
		data.sentts = nowts;
		data.resent = 0;
		data.resendts = 0;
		data.rto = 0;
		data.ack = false;
		std::memcpy(data.data.data(), static_cast<const char*>(buffer) + index, data.len);
		index += data.len;
	}
	// 输入了数据，ack立即发送
	if (index != 0 && !ackQueue_.empty())
	{
		ackNextTs_ = nowts;
	}
	return index;
}

std::size_t ReliableBuffer::fullSendBuffer(uint32_t nowts)
{
	const void* data = 0;
	std::size_t dataLength = 0;
	std::size_t putBytes = 0;
	std::size_t totalPutBytes = 0;
	while (putBytes == dataLength && putBuffer_.size() > 0)
	{
		dataLength = putBuffer_.data(data);
		putBytes = putSendBuffer(nowts, data, dataLength);
		putBuffer_.consume(putBytes);
		totalPutBytes += putBytes;
	}
	return totalPutBytes;
}

void ReliableBuffer::resetResentts()
{
	for (std::size_t i = 0; i != sendBuffer_.size(); ++i)
	{
		DatagramSendData& data = sendBuffer_[i];
		if (!data.ack && rtoCalc_.rto() < data.rto)
		{
			data.resendts -= data.rto;
			data.rto = rtoCalc_.rto();
			data.resendts += data.rto;
		}
	}
}

void ReliableBuffer::push(uint32_t nowts, const DatagramHead& head, uint32_t sn, uint16_t len, const void* buffer)
{
	// 序号错误
	if (sn == std::numeric_limits<uint32_t>::max() || recvWindowSize_ == 0)
	{
		return;
	}
	// 下一个可写入的包
	uint32_t recvNextSn = calcNextSequenceNo(recvEarliestSn_, static_cast<uint32_t>(recvBuffer_.size()));
	if (recvBuffer_.size() == recvWindowSize_)
	{
		recvNextSn -= 1;
	}
	// 写入应答
	if (needAckSequenceNo(recvNextSn, sn, recvWindowSize_))
	{
		putAck(nowts, head, sn);
	}
	uint32_t diffSn = calcDiffSequenceNo(recvEarliestSn_, sn);
	// 包超出窗口大小
	if (diffSn + 1 > recvWindowSize_)
	{
		return;
	}
	// 数据写入位置
	while (!recvBuffer_.full() && recvBuffer_.size() <= diffSn)
	{
		DatagramRecvData& data = recvBuffer_.push();
		data.available = false;
	}
	// 写入数据
	DatagramRecvData& data = recvBuffer_[diffSn];
	if (!data.available)
	{
		data.sn = sn;
		data.len = std::min(maxPacketLength_, len);
		std::memcpy(data.data.data(), buffer, data.len);
		data.available = true;
	}
}

void ReliableBuffer::putAck(uint32_t nowts, const DatagramHead& head, uint32_t sn)
{
	// 已经有ack，不需要重复添加
	bool needAck = true;
	for (std::size_t i = 0; i != ackQueue_.size(); ++i)
	{
		if (ackQueue_[i].sn == sn)
		{
			needAck = false;
			break;
		}
	}
	// 需要Ack
	if (needAck)
	{
		// 下一次ack发送时间
		if (ackQueue_.empty())
		{
			ackNextTs_ = nowts + (ackDelayMode_ ? ackDelayTime_ : 0);
		}
		// 入队
		if (ackQueue_.full())
		{
			ackQueue_.pop();
		}
		DatagramAck& ack = ackQueue_.push();
		ack.sn = sn;
		ack.ts = head.ts;
	}
}

void ReliableBuffer::ack(uint32_t nowts, const DatagramHead& head, uint32_t sn, uint32_t ts)
{
	// 增加窗口大小
	if (sendWindowSize_ < maxWindowSize_)
	{
		sendWindowSize_ = std::min<uint16_t>(static_cast<uint16_t>(sendWindowSize_ * 2), maxWindowSize_);
	}
	// 更新rto
	if (ts <= nowts)
	{
		rtoCalc_.update(nowts - ts);
	}
	// buffer为空
	if (sendBuffer_.empty())
	{
		return;
	}
	assert(sendEarliestSn_ == sendBuffer_.top().sn);
	uint32_t diffSn = calcDiffSequenceNo(sendEarliestSn_, sn);
	// 包还没有被发送
	if (diffSn + 1 > sendBuffer_.size())
	{
		return;
	}
	DatagramSendData& data = sendBuffer_[diffSn];
	// 包已经应答
	if (data.ack)
	{
		return;
	}
	// 设置应答状态
	data.ack = true;
	// 清空已经应答的缓冲区
	while (!sendBuffer_.empty() && sendBuffer_.top().ack)
	{
		assert(sendEarliestSn_ == sendBuffer_.top().sn);
		packLossRatePingValue(nowts, sendBuffer_.top());
		sendBuffer_.pop();
		sendEarliestSn_ = calcNextSequenceNo(sendEarliestSn_);
	}
}

void ReliableBuffer::una(uint32_t nowts, const DatagramHead& head)
{
	// 弹出对方已接受的包
	uint32_t diffSn = calcDiffSequenceNo(sendEarliestSn_, head.una);
	if (diffSn <= sendBuffer_.size())
	{
		while (!sendBuffer_.empty() && diffSn > 0)
		{
			assert(sendEarliestSn_ == sendBuffer_.top().sn);
			packLossRatePingValue(nowts, sendBuffer_.top());
			sendBuffer_.pop();
			sendEarliestSn_ = calcNextSequenceNo(sendEarliestSn_);
			--diffSn;
		}
	}
}

bool ReliableBuffer::parsePush(uint32_t nowts, const DatagramHead& head, const void* buffer, std::size_t len, std::size_t& index)
{
	uint32_t sn;
	uint16_t pushlen;
	if (index + sizeof(sn) + sizeof(pushlen) <= len)
	{
		index = DatagramDeserialize(sn, buffer, index);
		index = DatagramDeserialize(pushlen, buffer, index);
		// data
		if (index + pushlen > len)
		{
			return false;
		}
		push(nowts, head, sn, pushlen, static_cast<const char*>(buffer) + index);
		index += pushlen;
		return true;
	}
	return false;
}

bool ReliableBuffer::parseAck(uint32_t nowts, const DatagramHead& head, const void* buffer, std::size_t len, std::size_t& index)
{
	uint32_t sn;
	uint32_t ts;
	if (index + sizeof(sn) + sizeof(ts) <= len)
	{
		index = DatagramDeserialize(sn, buffer, index);
		index = DatagramDeserialize(ts, buffer, index);
		ack(nowts, head, sn, ts);
		return true;
	}
	return false;
}

void ReliableBuffer::packLossRatePingValue(uint32_t nowTs, const DatagramSendData& data)
{
	double thisPackLossRate = 1.0 * (data.resent - 1) / data.resent;
	double thisPackPingValue = nowTs > data.sentts ? nowTs - data.sentts : 0;
	if (recordPackCount_ == 0)
	{
		packLossRate_ = thisPackLossRate;
		packPingValue_ = thisPackPingValue;
	}
	if (recordPackCount_ < 5)
	{
		recordPackCount_ += 1;
	}
	packLossRate_ = (packLossRate_ * (recordPackCount_ - 1) + thisPackLossRate) / recordPackCount_;
	packPingValue_ = (packPingValue_ * (recordPackCount_ - 1) + thisPackPingValue) / recordPackCount_;
}
