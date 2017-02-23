#pragma once


#include <sys/socket.h>

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>


#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/Socket.h>






namespace muduo {

namespace net {
    class UdpService : public boost::noncopyable {
        public:
            UdpService(const InetAddress& localaddr);
            ~UdpService();

        private:
    };


}

}
