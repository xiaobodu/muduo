#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

namespace muduo {
namespace net {
namespace rudp {

// 循环缓冲区 
template <typename T>
class ReliableRingBuffer
{
public:

	ReliableRingBuffer(std::size_t maxSize)
		: buffer_(maxSize),
		startIndex_(0),
		stopIndex_(0),
		size_(0)
	{}

	~ReliableRingBuffer()
	{}

	T& operator[](std::size_t index)
	{
		assert(index < size_);
		return buffer_[(startIndex_ + index) % buffer_.size()];
	}

	const T& operator[](std::size_t index) const
	{
		assert(index < size_);
		return buffer_[(startIndex_ + index) % buffer_.size()];
	}

	std::size_t size() const
	{
		return size_;
	}

	bool empty() const
	{
		return size_ == 0;
	}

	bool full() const
	{
		return size_ == buffer_.size();
	}

	std::size_t max_size() const
	{
		return buffer_.size();
	}

	void push(const T& element)
	{
		assert(size_ < buffer_.size());
		buffer_[stopIndex_] = element;
		stopIndex_ = (stopIndex_ + 1) % buffer_.size();
		size_ += 1;
	}

	T& push()
	{
		assert(size_ < buffer_.size());
		std::size_t savedIndex = stopIndex_;
		stopIndex_ = (stopIndex_ + 1) % buffer_.size();
		size_ += 1;
		return buffer_[savedIndex];
	}

	T& top()
	{
		assert(size_ != 0);
		return buffer_[startIndex_];
	}

	const T& top() const
	{
		assert(size_ != 0);
		return buffer_[startIndex_];
	}

	T& tail()
	{
		assert(size_ != 0);
		return stopIndex_ != 0 ? buffer_[stopIndex_ - 1] : buffer_.back();
	}

	const T& tail() const
	{
		assert(size_ != 0);
		return stopIndex_ != 0 ? buffer_[stopIndex_ - 1] : buffer_.back();
	}

	void pop()
	{
		assert(size_ != 0);
		startIndex_ = (startIndex_ + 1) % buffer_.size();
		size_ -= 1;
	}

	void clear()
	{
		startIndex_ = 0;
		stopIndex_ = 0;
		size_ = 0;
	}

	void reserve(std::size_t maxSize)
	{
		if (buffer_.size() < maxSize)
		{
			std::vector<T> buffer;
			buffer.reserve(maxSize);
			for (std::size_t i = 0; i != size_; ++i)
			{
				buffer.push_back(buffer_[(startIndex_ + i) % buffer_.size()]);
			}
			while (buffer.size() < maxSize)
			{
				buffer.push_back(T());
			}
			buffer_.swap(buffer);
			startIndex_ = 0;
			stopIndex_ = size_;
		}
	}

private:
	std::vector<T> buffer_;
	std::size_t startIndex_;
	std::size_t stopIndex_;
	std::size_t size_;
};

}
}
}