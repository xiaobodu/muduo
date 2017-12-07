
#include <netinet/ip.h>
#include <netdb.h>

#include <muduo/base/Logging.h>


#include "UdpSocket.h"

#define HANDLE_EINTR(x) \
    ({ \
        decltype(x) eintrWrapperResult; \
        do { \
            eintrWrapperResult = (x);\
        } while (eintrWrapperResult == -1 && errno == EINTR); \
        eintrWrapperResult; \
     })

#define IGNORE_EINTR(x) \
    ({ \
        decltype(x) eintrWrapperResult; \
        do { \
            eintrWrapperResult = (x); \
            if (eintrWrapperResult == -1 && errno == EINTR) { \
                eintrWrapperResult = 0; \
            } \
        } while (0); \
        eintrWrapperResult; \
     })


using namespace muduo;
using namespace muduo::net;


UdpSocket::UdpSocket()
    : sockfd_(kInvalidSocket),
      addrFamily_(AF_UNSPEC),
      socketOptions_(0) {}

UdpSocket::~UdpSocket() {
    Close();
}

int UdpSocket::Connect(const InetAddress& address) {
    assert(!IsConnected());
    assert(!remoteAddress_.get());

    int addrFamily = address.family();
    int rv = CreateSocket(addrFamily);
    if (rv < 0) {
        return rv;
    }

    SockaddrStorage storage;
    if (!SockaddrStorage::ToSockAddr(address, &storage)) {
        Close();
        return EADDRNOTAVAIL;
    }

    rv = HANDLE_EINTR(::connect(sockfd_, storage.Addr, storage.AddrLen));
    if (rv < 0) {
        int lastError = errno;
        Close();
        return lastError;
    }

    remoteAddress_.reset(new InetAddress(address));
    return rv;
}

int UdpSocket::Bind(const InetAddress& address) {
    assert(!IsConnected());

    int rv = CreateSocket(address.family());
    if (rv < 0){
        return rv;
    }

    rv = SetSocketOptions(); // last error
    if (rv < 0) {
        Close();
        return rv;
    }

    rv = DoBind(address);
    if (rv < 0) {
        Close();
        return rv;
    }
    localAddress_.reset();
    return rv;
}

int UdpSocket::DoBind(const InetAddress& address) {
    assert(IsConnected());


    SockaddrStorage storage;

    if (!SockaddrStorage::ToSockAddr(address, &storage)) {
        return -1;
    }

    int rv = ::bind(sockfd_, storage.Addr, storage.AddrLen);
    int lastError = errno;
    if (rv < 0) {
        LOG_SYSERR << "::bind";
    }

    return rv == 0 ? 0 : lastError;
}


int UdpSocket::CreateSocket(int addrFamily) {
    assert(sockfd_ == kInvalidSocket);

    int sockfd = ::socket(addrFamily, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                         IPPROTO_UDP);

    if (sockfd == kInvalidSocket) {
        int lastError = errno;
        LOG_SYSERR << "::socket";
        return lastError;
    }

    addrFamily_ = addrFamily;
    sockfd_ = sockfd;

    return 0;
}


int UdpSocket::SetSocketOptions() {
    assert(IsConnected());

    int trueVaue = 1;
    if (socketOptions_ & SOCKET_OPTION_REUSE_ADDRESS) {
        int rv = setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &trueVaue,
                            sizeof(trueVaue));
        if (rv < 0) {
            return errno;
        }
    }

    if (socketOptions_ & SOCKET_OPTION_REUSE_PORT) {
        int rv = setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &trueVaue,
                            sizeof(trueVaue));
        if (rv < 0) {
            return errno;
        }
    }


    if (socketOptions_ & SOCKET_OPTION_IPTOS_LOWDELAY) {
        int rv = setsockopt(sockfd_, IPPROTO_IP, IP_TOS, &trueVaue,
                            sizeof(trueVaue));
        if (rv < 0) {
            return errno;
        }
    }
    return 0;
}

void UdpSocket::AllowAddressReuse() {
    assert(!IsConnected());
    socketOptions_ |= SOCKET_OPTION_REUSE_ADDRESS;
}

void UdpSocket::AllowPortResuse() {
    assert(!IsConnected());
    socketOptions_ |= SOCKET_OPTION_REUSE_PORT;
}

void UdpSocket::AllowTosWithLowDelay() {
    assert(!IsConnected());
    socketOptions_ |= SOCKET_OPTION_IPTOS_LOWDELAY;
}


int UdpSocket::SetReceiveBufferSize(int32_t size) {
    assert(IsConnected());
    int rv = setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF,
                        reinterpret_cast<const char*>(&size), sizeof(size));
    int lastError = errno;
    if (rv < 0) {
        LOG_SYSERR << "::setsockopt";
    }

    return rv == 0 ? 0 : lastError;
}

int UdpSocket::SetSendBufferSize(int32_t size) {
    assert(IsConnected());

    int rv = setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF,
                        reinterpret_cast<const char*>(&size), sizeof(size));
    int lastError = errno;
    if (rv < 0) {
        LOG_SYSERR << "::setsockopt";
    }

    return rv == 0 ? 0 : lastError;
}


ssize_t UdpSocket::Read(void* buf, size_t len) {
    return RecvFrom(buf, len, NULL);
}

ssize_t UdpSocket::RecvFrom(void* buf, size_t len, InetAddress* address) {
    ssize_t nr;
    int flags = 0;

    SockaddrStorage storage;

    nr = HANDLE_EINTR(::recvfrom(sockfd_, buf, len, flags, storage.Addr, &storage.AddrLen));

    ssize_t result;
    if (nr >=0 ) {
        if (address != NULL && !SockaddrStorage::ToInetAddr(storage, address)) {
            result = EINVAL;
        } else {
            result = nr;
        }
    } else {
        int lastError = errno;
        if (lastError != EAGAIN && lastError != EWOULDBLOCK) {
            LOG_SYSERR << "::recvfrom";
        }
        result = lastError;
    }

    return result;
}

ssize_t UdpSocket::Write(const void* buf, size_t len) {
    return SendToOrWrite(buf, len, NULL);
}

ssize_t UdpSocket::SendTo(const void* buf, size_t len,
                      const InetAddress& address) {
    return SendToOrWrite(buf, len, &address);
}

ssize_t UdpSocket::SendToOrWrite(const void* buf, size_t len,
                             const InetAddress* address) {
    assert(IsConnected());
    assert(len > 0);

    ssize_t result = InternalSendTo(buf, len, address);
    if (result != EAGAIN && result != EWOULDBLOCK) {
        return result;
    }

    return result;
}


ssize_t UdpSocket::InternalSendTo(const void* buf, size_t len,
                              const InetAddress* address) {
    SockaddrStorage storage;
    struct sockaddr* addr = storage.Addr;
    if (!address) {
        addr = NULL;
        storage.AddrLen = 0;
    } else {
        if (!SockaddrStorage::ToSockAddr(*address, &storage)) {
            return EFAULT;
        }
    }

    int flags = 0;
    ssize_t result = HANDLE_EINTR(::sendto(sockfd_, buf, len, flags, addr, storage.AddrLen));
    if (result < 0) {
        int lastError = errno;
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_SYSERR << "::sendto";
        }

        return lastError;
    }

    return result;
}


void UdpSocket::Close() {
    if (!IsConnected()) {
        return;
    }

    if (IGNORE_EINTR(::close(sockfd_)) < 0) {
        LOG_SYSERR << "::close";
    }

    sockfd_ = kInvalidSocket;
    addrFamily_ = AF_UNSPEC;
}

int UdpSocket::localAddress(InetAddress* address) const {
    assert(address != NULL);

    if (!IsConnected()) {
        return ENOTCONN;
    }

    if (!localAddress_.get()) {
        SockaddrStorage storage;
        if (getsockname(sockfd_, storage.Addr, &storage.AddrLen) < 0) {
            return errno;
        }

        boost::scoped_ptr<InetAddress> inetAddr(new InetAddress());
        if (!SockaddrStorage::ToInetAddr(storage, inetAddr.get())) {
            return EADDRNOTAVAIL;
        }

        localAddress_.swap(inetAddr);
    }

    *address = *localAddress_;

    return 0;
}

int UdpSocket::peerAddress(InetAddress* address) const {
    assert(address != NULL);

    if (!IsConnected()) {
        return ENOTCONN;
    }

    if (!remoteAddress_.get()) {
        SockaddrStorage storage;
        if (getpeername(sockfd_, storage.Addr, &storage.AddrLen) < 0) {
            return errno;
        }

        boost::scoped_ptr<InetAddress> inetAddr(new InetAddress());
        if (!SockaddrStorage::ToInetAddr(storage, inetAddr.get())) {
            return EADDRNOTAVAIL;
        }

        remoteAddress_.swap(inetAddr);
    }

    *address = *remoteAddress_;
    return 0;
}
