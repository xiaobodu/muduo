#pragma once

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/Types.h>




namespace muduo {

namespace net {
    class UdpMessage : public boost::noncopyable {
        public:
            UdpMessage(std::size_t defaultBufferSize = 1472);

            struct sockaddr_in& formAddr() { return fromAddr_; }
            string toIpPort() const;
            Buffer& buffer() { return buffer_; }
        private:
            struct sockaddr_in fromAddr_;
            Buffer buffer_;
    };
}

}






