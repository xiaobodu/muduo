#pragma once

#include <stddef.h>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <muduo/net/InetAddress.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/Types.h>




namespace muduo {

namespace net {
    class UdpMessage;
    typedef boost::shared_ptr<UdpMessage> UdpMessagePtr;
    class UdpMessage : public boost::noncopyable {
        public:
            UdpMessage(const boost::shared_ptr<Buffer>& buf, const InetAddress& inetAddr);
            UdpMessage(const char* buf, size_t len, const InetAddress& intetAddr);

            // source intetAddress with message recv
            // destination intetAddress with message send;
            const InetAddress& intetAddress() const { return intetAddress_; }
            boost::shared_ptr<Buffer>&  buffer() { return buffer_; }
        private:
            InetAddress intetAddress_;
            boost::shared_ptr<Buffer> buffer_;
    };
}

}






