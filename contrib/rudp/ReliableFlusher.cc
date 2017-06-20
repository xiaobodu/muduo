#include "ReliableFlusher.h"

using namespace muduo::net::rudp;


ReliableFlusher::ReliableFlusher(uint16_t mss)
	: mss_(mss),
	dataIndex_(0),
	size_(0)
{}

ReliableFlusher::~ReliableFlusher()
{}

void ReliableFlusher::setFlushHandler(const ReliableFlushHandler& handler)
{
	handler_ = handler;
}

void ReliableFlusher::setMssSize(uint16_t mss)
{
	mss_ = mss;
}

uint16_t ReliableFlusher::getMssSize() const
{
	return mss_;
}

std::size_t ReliableFlusher::size() const
{
	return size_;
}

bool ReliableFlusher::ensure(std::size_t n) const
{
	return size_ + n <= mss_;
}

void ReliableFlusher::start(const DatagramHead& head)
{
	clear();
	std::size_t savedIndex = dataIndex_;
	dataIndex_ = DatagramSerialize(head.ts,  data_, dataIndex_);
	dataIndex_ = DatagramSerialize(head.una, data_, dataIndex_);
	dataIndex_ = DatagramSerialize(head.wnd, data_, dataIndex_);
	size_ += dataIndex_ - savedIndex;
}

void ReliableFlusher::push(const DatagramHead& head, const DatagramAck& ack)
{
	check(head, DATAGRAM_CMD_SIZEOF + DATAGRAM_ACK_SIZEOF);
	uint8_t cmd = DATAGRAM_CMD_ACK;
	std::size_t savedIndex = dataIndex_;
	dataIndex_ = DatagramSerialize(cmd,    data_, dataIndex_);
	dataIndex_ = DatagramSerialize(ack.sn, data_, dataIndex_);
	dataIndex_ = DatagramSerialize(ack.ts, data_, dataIndex_);
	size_ += dataIndex_ - savedIndex;
}

void ReliableFlusher::push(const DatagramHead& head, const DatagramSendData& data)
{
	check(head, DATAGRAM_CMD_SIZEOF + DATAGRAM_DATA_HEAD_SIZEOF + data.len);
	uint8_t cmd = DATAGRAM_CMD_PSH;
	std::size_t savedIndex = dataIndex_;
	dataIndex_ = DatagramSerialize(cmd,      data_, dataIndex_);
	dataIndex_ = DatagramSerialize(data.sn,  data_, dataIndex_);
	dataIndex_ = DatagramSerialize(data.len, data_, dataIndex_);
    dataIndex_ = DatagramSerialize(data.data.data(), data.len, data_, dataIndex_);
	size_ += dataIndex_ - savedIndex + data.len;
}

void ReliableFlusher::push(const DatagramHead& head, const DatagramSync& sync)
{
	check(head, DATAGRAM_CMD_SIZEOF + DATAGRAM_SYNC_SIZEOF);
	uint8_t cmd = DATAGRAM_CMD_SYN;
	std::size_t savedIndex = dataIndex_;
	dataIndex_ = DatagramSerialize(cmd,     data_, dataIndex_);
	dataIndex_ = DatagramSerialize(sync.sn, data_, dataIndex_);
	size_ += dataIndex_ - savedIndex;
}

void ReliableFlusher::push(const DatagramHead& head, const DatagramFinsh& finsh)
{
	check(head, DATAGRAM_CMD_SIZEOF + DATAGRAM_FIN_SIZEOF);
	uint8_t cmd = DATAGRAM_CMD_FIN;
	std::size_t savedIndex = dataIndex_;
	dataIndex_ = DatagramSerialize(cmd, data_, dataIndex_);
	size_ += dataIndex_ - savedIndex;
}

void ReliableFlusher::end()
{
	flush();
}

void ReliableFlusher::clear()
{
	dataIndex_ = 0;
	size_ = 0;
}

void ReliableFlusher::check(const DatagramHead& head, std::size_t n)
{
	if (!ensure(n))
	{
		flush();
		start(head);
	}
}

void ReliableFlusher::flush()
{
	if (handler_)
	{
		ReliableFlushHandler handler = handler_;
		handler(data_, size_);
	}
	clear();
}
