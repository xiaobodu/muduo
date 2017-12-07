#pragma once

#include <array>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>


#include "ReliableDefine.h"

namespace muduo {
namespace net {
namespace rudp {

// 输出回调
class ReliableFlusher;
typedef boost::function<void(const void*, std::size_t)> ReliableFlushHandler;

// 缓冲区刷新
class ReliableFlusher : boost::noncopyable
{
public:

	 explicit ReliableFlusher(uint16_t mss);

	~ReliableFlusher();

	// 设置输出处理器
	void setFlushHandler(const ReliableFlushHandler& handler);

	// 设置mss大小
	void setMssSize(uint16_t mss);

	// mss大小
	uint16_t getMssSize() const;

	// 是否还能写入n
	bool ensure(std::size_t n) const;

	// 数据长度
	std::size_t size() const;

	// 准备
	void start(const DatagramHead& head);

	// 应答
	void push(const DatagramHead& head, const DatagramAck& ack);

	// 数据
	void push(const DatagramHead& head, const DatagramSendData& data);

	// 同步指令
	void push(const DatagramHead& head, const DatagramSync& sync);

	// 结束指令
	void push(const DatagramHead& head, const DatagramFinsh& finsh);

	// 结束
	void end();

	// 清空
	void clear();

private:
	// 检查是否需要立即刷新
	void check(const DatagramHead& head, std::size_t n);

	// 刷新
	void flush();

	// mss
	uint16_t mss_;
	// 数据
	char data_[2048];
	std::size_t dataIndex_;
	std::size_t size_;

	// 输出
	ReliableDataBuffers buffers_;
	// 刷新输出
	ReliableFlushHandler handler_;
};

}
}
}
