#pragma once



#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>




#include <muduo/net/InetAddress.h>





namespace muduo {
namespace net {
namespace rudp {

const socklen_t kSockaddrInSize = sizeof(struct sockaddr_in);
const socklen_t kSockaddrIn6Size = sizeof(struct sockaddr_in6);
const int kInvalidSocket = -1;
const size_t KDefaultMaxPacketSize = 1472;

struct SockaddrStorage {
    SockaddrStorage()
        : AddrLen(sizeof(AddrStorage)),
          Addr(reinterpret_cast<struct sockaddr*>(&AddrStorage)){}

    SockaddrStorage(const SockaddrStorage& other)
        : AddrLen(other.AddrLen),
          Addr(reinterpret_cast<struct sockaddr*>(&AddrStorage)){
        ::memcpy(Addr, other.Addr, AddrLen);
    }

    void operator=(const SockaddrStorage& other) {
        AddrLen = other.AddrLen;
        ::memcpy(Addr, other.Addr, AddrLen);
    }

    static bool ToSockAddr(const InetAddress& address,
                           SockaddrStorage* storage) {
        assert(storage != NULL);
        int addrFamily = address.family();
        switch (addrFamily) {
            case AF_INET: {
                const struct sockaddr_in* addr =
                    reinterpret_cast<const struct sockaddr_in*>(address.getSockAddr());
                memcpy(storage->Addr, addr, kSockaddrInSize);
                storage->AddrLen = kSockaddrInSize;
                break;
            }
            case AF_INET6: {
                const struct sockaddr_in6* addr =
                    reinterpret_cast<const struct sockaddr_in6*>(address.getSockAddr());
                memcpy(storage->Addr, addr, kSockaddrIn6Size);
                storage->AddrLen = kSockaddrIn6Size;
                break;
            }
            default:
                return false;
        }

        return true;
    }

    static bool ToInetAddr(const SockaddrStorage& storage,
                           InetAddress* address) {
        assert(address != NULL);
        int addrLen = storage.AddrLen;
        switch (addrLen) {
            case kSockaddrInSize:
            case kSockaddrIn6Size: {
                const struct sockaddr_in6* addr =
                    reinterpret_cast<const struct sockaddr_in6*>(storage.Addr);
                address->setSockAddrInet6(*addr);
                break;
            }
            default:
                return false;
        }
        return true;
    }

    struct sockaddr_storage AddrStorage;
    socklen_t AddrLen;
    struct sockaddr* const Addr;
};

    class UdpSocket : public boost::noncopyable {
        public:
        public:
            UdpSocket();
            ~UdpSocket();

            int Connect(const InetAddress& address);
            int Bind(const InetAddress& address);
            void Close();

            int peerAddress(InetAddress* address) const;
            int localAddress(InetAddress* address) const;

            ssize_t Read(void* buf, size_t len);
            ssize_t Write(const void* buf, size_t len);

            ssize_t RecvFrom(void* buf, size_t len, InetAddress* address);

            ssize_t SendTo(const void* buf, size_t len, const InetAddress& address);
            ssize_t SendToOrWrite(const void* buf, size_t len, const InetAddress* address);

            void AllowAddressReuse();
            void AllowPortResuse();
            void AllowTosWithLowDelay();

            int SetReceiveBufferSize(int32_t size);
            int SetSendBufferSize(int32_t size);

            int sockfd() const { return sockfd_; }

            bool IsConnected() const { return sockfd_ != kInvalidSocket; }
        private:
            enum SocketOptions {
                SOCKET_OPTION_REUSE_ADDRESS = 1 << 0,
                SOCKET_OPTION_REUSE_PORT = 1 << 1,
                SOCKET_OPTION_IPTOS_LOWDELAY = 1 << 2,
            };

            int CreateSocket(int addrFamily);
            int SetSocketOptions();
            int DoBind(const InetAddress& address);
            ssize_t InternalSendTo(const void* buf, size_t len,
                               const InetAddress* address);
        private:
            int sockfd_;
            int addrFamily_;
            int socketOptions_;


            mutable boost::scoped_ptr<InetAddress> localAddress_;
            mutable boost::scoped_ptr<InetAddress> remoteAddress_;
    };
}
}
}
