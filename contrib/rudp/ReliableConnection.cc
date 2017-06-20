#include "ReliableConnection.h"

#include <boost/bind.hpp>
#include <boost/typeof/typeof.hpp>

#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::rudp;

static  const int kMicroSecondsPerMillissecond = 1000;
static  const int kMillissecondsPerSecond = 1000;

static inline Timestamp AddTime(Timestamp timestamp, int64_t millisecond) {
    int64_t delta = millisecond * kMicroSecondsPerMillissecond;
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

ReliableConnection::ReliableConnection(EventLoop* loop, uint16_t maxWindowSize, uint16_t mss, uint16_t maxPackLength)
	: loop_(loop),
	status_(0),
    baseTime_(Timestamp::now()),
	buffer_(maxWindowSize, maxPackLength),
	flush_(mss),
	maxPacketLength_(maxPackLength),
	redundant_(0)
{}

ReliableConnection::~ReliableConnection()
{}

uint16_t ReliableConnection::getMssSize() const
{
	return flush_.getMssSize();
}

uint32_t ReliableConnection::getNowTs() const
{
    uint64_t diff = static_cast<uint64_t>(Timestamp::now().microSecondsSinceEpoch() - baseTime_.microSecondsSinceEpoch());
    return static_cast<uint32_t>((diff / kMicroSecondsPerMillissecond) & 0xfffffffful);
}

uint32_t ReliableConnection::getNowRtt() const
{
	return buffer_.getNowRtt();
}

uint32_t ReliableConnection::getAverageRtt() const
{
	return buffer_.getAverageRtt();
}

uint32_t ReliableConnection::getNowRto() const
{
	return buffer_.getNowRto();
}

uint32_t ReliableConnection::getPackLossRate() const
{
	return buffer_.getPackLossRate();
}

uint32_t ReliableConnection::getPackPingValue() const
{
	return buffer_.getPackPingValue();
}

void ReliableConnection::setRtoBound(uint32_t minRto, uint32_t maxRto)
{
	buffer_.setRtoBound(minRto, maxRto);
}

void ReliableConnection::setAckDelay(bool delay, uint32_t delayMills)
{
	buffer_.setAckDelay(delay, delayMills);
}

void ReliableConnection::setRedundant(uint16_t n)
{
	redundant_ = n;
}

void ReliableConnection::establish()
{
	status_ |= kEstablish | kRunning;
	startUpdateTimer();
}

void ReliableConnection::destroy()
{
	status_ &= ~kEstablish;
    loop_->cancel(updateTimer_);
	packetHander_ = 0;
	flush_.setFlushHandler(0);
}

void ReliableConnection::setRunning(bool running)
{
	uint32_t status = status_;
	status_ |= kRunning;
	if ((status_ & kEstablish) && !(status & kRunning) && running)
	{
		startUpdateTimer();
	}
}

bool ReliableConnection::getRunning() const
{
	return status_ & kRunning;
}

void ReliableConnection::process(const void* data, std::size_t n)
{
	if (status_ & kEstablish)
	{
		uint32_t nowTs = getNowTs();
		buffer_.input(nowTs, data, n);
		status_ |= kRunning;
		immediatelyUpdate(nowTs);
	}
}

void ReliableConnection::send(const void* data, std::size_t n)
{
	if (status_ & kEstablish)
	{
		uint32_t nowTs = getNowTs();
		bool needUpdate = buffer_.put(nowTs, data, n);
		if (needUpdate && (status_ & kRunning) && !(status_ & kUpdating))
		{
			postImmediatelyUpdate();
		}
	}
}

void ReliableConnection::setFlushHandler(const ReliableFlushHandler& handler)
{
	flush_.setFlushHandler(handler);
}

void ReliableConnection::setPacketHandler(const PacketHandler& handler)
{
	packetHander_ = handler;
}

void ReliableConnection::immediatelyUpdate()
{
	uint32_t nowTs = getNowTs();
	immediatelyUpdate(nowTs);
}

void ReliableConnection::immediatelyUpdate(uint32_t nowTs)
{
	status_ &= ~kPosting;
	update(nowTs, false);
}

void ReliableConnection::postImmediatelyUpdate()
{
	if (!(status_ & kPosting))
	{
		status_ |= kPosting;
		loop_->queueInLoop(boost::bind(&ReliableConnection::immediatelyUpdate, shared_from_this()));
	}
}

void ReliableConnection::update(uint32_t nowTs, bool fromTimer)
{	
	// update逻辑
	uint32_t nextTs = nowTs;
	while (nextTs <= nowTs && (status_ & kEstablish))
	{
		// 处理buffer
		nextTs = buffer_.update(nowTs, flush_, redundant_);
		// 读数据
		status_ |= (nextTs <= nowTs && (status_ & kEstablish)) ? kUpdating : 0;

		doRead();
		status_ &= ~kUpdating;
	}
	Timestamp nextTimerTime = AddTime(baseTime_, nextTs);
	// 继续投递
	if ((status_ & kEstablish) && (status_ & kRunning))
	{
		bool needPostTimer = fromTimer;
		if (!needPostTimer && nextTimerTime < nextTimerTime_)
		{
            loop_->cancel(updateTimer_);
			needPostTimer = true;
		}
		if (needPostTimer)
		{
            nextTimerTime_ = nextTimerTime;
            updateTimer_ = loop_->runAt(nextTimerTime_, boost::bind(&ReliableConnection::onUpdateTimer, shared_from_this()));
		}
	}
}

void ReliableConnection::startUpdateTimer()
{
	uint32_t nowTs = getNowTs();
	uint32_t nextTs = buffer_.check(nowTs);
    nextTimerTime_ = AddTime(baseTime_, nextTs);
    updateTimer_ = loop_->runAt(nextTimerTime_, boost::bind(&ReliableConnection::onUpdateTimer, shared_from_this()));
}

void ReliableConnection::onUpdateTimer()
{
	if (status_ & kEstablish)
	{
		uint32_t nowTs = getNowTs();
		update(nowTs, true);
	}
}

void ReliableConnection::doRead()
{
	const void* buffer = 0;
	std::size_t nbytes = 0;

	while (buffer_.top(buffer, nbytes) && packetHander_)
	{
		PacketHandler handler = packetHander_;
		handler(buffer, nbytes);
		buffer_.pop();
	}
}
