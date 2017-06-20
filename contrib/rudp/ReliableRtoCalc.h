#pragma once

#include <stdint.h>

namespace muduo {
namespace net {
namespace rudp {

// rto 计算器
class ReliableRtoCalc
{
public:

	ReliableRtoCalc(uint32_t minRto, uint32_t maxRto, uint32_t rto);

	~ReliableRtoCalc();

	// 设置rto范围
	void setRtoBound(uint32_t minRto, uint32_t maxRto);

	// 更新rto
	void update(uint32_t rtt);

	// 最小rto
	uint32_t minRto() const;

	// 最大rto
	uint32_t maxRto() const;

	// 网络往返
	uint32_t rtt() const;

	// 平均网络往返
	uint32_t averageRtt() const;

	// 网络往返
	uint32_t rto() const;

	// 下一次rto时间
	uint32_t nextRto(uint32_t prevRto);

private:
	// 最小rto
	uint32_t minRto_;
	// 最大rto
	uint32_t maxRto_;
	// 网络往返
	uint32_t rtt_;
	// 平均网络往返
	uint32_t averageRtt_;
	// 重传超时时间
	uint32_t rto_;
	float srtt_;
	float rttval_;
};

}
}
}