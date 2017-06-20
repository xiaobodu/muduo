#pragma once

#include <stdint.h>
#include <deque>
#include <string>

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <muduo/net/TimerId.h>
#include <muduo/base/Timestamp.h>

#include "ReliableByteBuffer.h"
#include "ReliableBuffer.h"
#include "ReliableFlusher.h"

namespace muduo {
namespace net {

class EventLoop;
namespace rudp {

class ReliableConnection;
typedef boost::shared_ptr<ReliableConnection> ReliableConnectionPtr;

// 可靠连接
class ReliableConnection : public boost::enable_shared_from_this<ReliableConnection>, boost::noncopyable
{
public:

	// 数据包处理器
	typedef boost::function<void(const void*, std::size_t)> PacketHandler;

	// conv 会话ID
	ReliableConnection(EventLoop* loop, uint16_t maxWindowSize, uint16_t mss, uint16_t maxPackLength);

	~ReliableConnection();

	// mss大小
	uint16_t getMssSize() const;

	// 对象创建以来的毫秒数
	uint32_t getNowTs() const;

	// 获取当前往返时间
	uint32_t getNowRtt() const;

	// 平均往返延迟
	uint32_t getAverageRtt() const;

	// 获取重传超时时间
	uint32_t getNowRto() const;

	// 丢包率,单位0.001
	uint32_t getPackLossRate() const;

	// 包ping值
	uint32_t getPackPingValue() const;

	// 设置rto范围
	void setRtoBound(uint32_t minRto, uint32_t maxRto);

	// 设置延迟模式，ack包会延迟发送
	void setAckDelay(bool delay, uint32_t delayMills = DATAGRAM_DELAY_TIME);

	// 设置是否冗余发送
	void setRedundant(uint16_t n);

	// 开始
	void establish();

	// 销毁
	void destroy();

	// 设置运行态
	void setRunning(bool running);

	// 获取运行态
	bool getRunning() const;

	// 处理输入数据
	void process(const void* data, std::size_t n);

	// 发送
	void send(const void* data, std::size_t n);

	// 设置输出回调
	void setFlushHandler(const ReliableFlushHandler& handler);

	// 设置消息包处理器
	void setPacketHandler(const PacketHandler& handler);

private:

	// 立即update
	void immediatelyUpdate();
	void immediatelyUpdate(uint32_t nowTs);

	// 提价update请求
	void postImmediatelyUpdate();

	// 更新buffer
	void update(uint32_t nowTs, bool fromTimer);

	// 发起update timer
	void startUpdateTimer();

	// update buffer 定时器
	void onUpdateTimer();

	// 读缓冲区
	void doRead();

	enum Status
	{
		// 就绪态
		kEstablish = 1,
		// 正在update
		kUpdating = 2,
		// 运行态
		kRunning = 4,
		// 投递了update
		kPosting = 8,
	};

    EventLoop* loop_;
	// 状态
	uint32_t status_;
	// update 定时器
    TimerId updateTimer_;
	// 基准时间
    Timestamp baseTime_;
    //下一次更新时间
    Timestamp nextTimerTime_;
	// 缓冲区
	ReliableBuffer buffer_;
	// 刷新器
	ReliableFlusher flush_;
	// 写入数据包大小
	uint16_t maxPacketLength_;
	// 冗余发送包
	uint16_t redundant_;
	// 消息包处理器
	PacketHandler packetHander_;
};

}
}
}
