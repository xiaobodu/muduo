
#include <cassert>
#include <cmath>
#include <algorithm>

#include "ReliableRtoCalc.h"

using namespace muduo::net::rudp;



ReliableRtoCalc::ReliableRtoCalc(uint32_t minRto, uint32_t maxRto, uint32_t rto)
	: minRto_(minRto),
	maxRto_(maxRto),
	rtt_(0),
	averageRtt_(0),
	rto_(rto),
	srtt_(0),
	rttval_(0.75)
{
	assert(rto >= minRto && rto <= maxRto);
}

ReliableRtoCalc::~ReliableRtoCalc()
{}

void ReliableRtoCalc::setRtoBound(uint32_t minRto, uint32_t maxRto)
{
	assert(minRto <= maxRto);
	minRto_ = minRto;
	maxRto_ = maxRto;
	rto_ = std::min(std::max(rto_, minRto_), maxRto_);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
// 更新rto
void ReliableRtoCalc::update(uint32_t rtt)
{


	rtt_ = rtt;
	// 计算rto
	double delta = rtt_ - srtt_;
	srtt_ += delta / 8.0;
	delta = std::abs(delta);
	rttval_ += (delta - rttval_) / 4.0;
	rto_ = srtt_ + (4.0 * rttval_) + 0.5;
	rto_ = std::min(std::max(rto_, minRto_), maxRto_);
	// 计算平均rtt
	if (averageRtt_ == 0)
	{
		averageRtt_ = rtt_;
	}
	averageRtt_ = static_cast<uint32_t>((averageRtt_ * 4.0 + rtt_) / 5.0);
}
#pragma GCC diagnostic pop
#pragma GCC diagnostic warning "-Wconversion"
#pragma GCC diagnostic warning "-Wfloat-conversion"

uint32_t ReliableRtoCalc::minRto() const
{
	return minRto_;
}

uint32_t ReliableRtoCalc::maxRto() const
{
	return maxRto_;
}

uint32_t ReliableRtoCalc::rtt() const
{
	return rtt_;
}

uint32_t ReliableRtoCalc::rto() const
{
	return rto_;
}

uint32_t ReliableRtoCalc::averageRtt() const
{
	return averageRtt_;
}

uint32_t ReliableRtoCalc::nextRto(uint32_t prevRto)
{
	if (prevRto != 0)
	{
		uint32_t halfRto = std::max<uint32_t>(rto_ / 2, 1);
		return std::min<uint32_t>(prevRto + halfRto, maxRto_);
	}
	return rto_;
}