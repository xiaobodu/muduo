#pragma once

#include <cstddef>
#include <cstring>
#include <vector>

namespace muduo {
namespace net {
namespace rudp {

class ReliableByteBuffer
{
public:

	ReliableByteBuffer();

	~ReliableByteBuffer();

	// 写入
	void put(const void* buffer, std::size_t n);

	// buffer内保存的数据
	std::size_t data(const void*& data);

	// 消耗字节数
	void consume(std::size_t n);

	// 清空
	void clear();

	// 总的字节数
	std::size_t size() const;

private:

	// 交换缓冲区
	void exchange();

	std::vector<char> firstBuffer_;
	std::size_t firstWriterIndex_;
	std::vector<char> secondBuffer_;
	std::size_t secondReaderIndex_;
	std::size_t secondWriterIndex_;
};

}
}
}