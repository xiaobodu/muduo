
#include <algorithm>

#include "ReliableByteBuffer.h"
using namespace muduo::net::rudp;


ReliableByteBuffer::ReliableByteBuffer()
	: firstWriterIndex_(0),
	secondReaderIndex_(0),
	secondWriterIndex_(0)
{}

ReliableByteBuffer::~ReliableByteBuffer()
{}

// 写入
void ReliableByteBuffer::put(const void* buffer, std::size_t n)
{
	if (firstWriterIndex_ + n > firstBuffer_.size())
	{
		std::size_t newBufferLength = std::max<std::size_t>(firstBuffer_.size() * 2,
			firstWriterIndex_ + n);
		firstBuffer_.resize(newBufferLength);
	}
	std::memcpy(firstBuffer_.data() + firstWriterIndex_, buffer, n);
	firstWriterIndex_ += n;
}

// buffer内保存的数据
std::size_t ReliableByteBuffer::data(const void*& data)
{
	if (secondReaderIndex_ == secondWriterIndex_)
	{
		exchange();
	}
	data = secondBuffer_.data() + secondReaderIndex_;
	return secondWriterIndex_ - secondReaderIndex_;
}

// 消耗字节数
void ReliableByteBuffer::consume(std::size_t n)
{
	std::size_t secondLength = secondWriterIndex_ - secondReaderIndex_;
	if (secondLength < n)
	{
		n -= secondLength;
		exchange();
		secondLength = secondWriterIndex_ - secondReaderIndex_;
	}
	secondReaderIndex_ += std::min(secondLength, n);
}

void  ReliableByteBuffer::clear()
{
	secondReaderIndex_ = 0;
	secondWriterIndex_ = 0;
	firstWriterIndex_ = 0;
}
// 总的字节数
std::size_t ReliableByteBuffer::size() const
{
	return secondWriterIndex_ - secondReaderIndex_ + firstWriterIndex_;
}

void ReliableByteBuffer::exchange()
{
	firstBuffer_.swap(secondBuffer_);
	secondReaderIndex_ = 0;
	secondWriterIndex_ = firstWriterIndex_;
	firstWriterIndex_ = 0;
}