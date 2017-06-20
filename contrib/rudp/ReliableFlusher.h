#pragma once

#include <array>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>


#include "ReliableDefine.h"

namespace muduo {
namespace net {
namespace rudp {

// ����ص�
class ReliableFlusher;
typedef boost::function<void(const void*, std::size_t)> ReliableFlushHandler;

// ������ˢ��
class ReliableFlusher : boost::noncopyable
{
public:

	 explicit ReliableFlusher(uint16_t mss);

	~ReliableFlusher();

	// �������������
	void setFlushHandler(const ReliableFlushHandler& handler);

	// ����mss��С
	void setMssSize(uint16_t mss);

	// mss��С
	uint16_t getMssSize() const;

	// �Ƿ���д��n
	bool ensure(std::size_t n) const;

	// ���ݳ���
	std::size_t size() const;

	// ׼��
	void start(const DatagramHead& head);

	// Ӧ��
	void push(const DatagramHead& head, const DatagramAck& ack);

	// ����
	void push(const DatagramHead& head, const DatagramSendData& data);

	// ͬ��ָ��
	void push(const DatagramHead& head, const DatagramSync& sync);

	// ����ָ��
	void push(const DatagramHead& head, const DatagramFinsh& finsh);

	// ����
	void end();

	// ���
	void clear();

private:
	// ����Ƿ���Ҫ����ˢ��
	void check(const DatagramHead& head, std::size_t n);

	// ˢ��
	void flush();

	// mss
	uint16_t mss_;
	// ����
	char data_[2048];
	std::size_t dataIndex_;
	std::size_t size_;

	// ���
	ReliableDataBuffers buffers_;
	// ˢ�����
	ReliableFlushHandler handler_;
};

}
}
}
