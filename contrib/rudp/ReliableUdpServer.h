#pragma once

#include <stdint.h>
#include <vector>



#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>
#include <boost/random.hpp>
#include <boost/unordered_set.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Types.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Mutex.h>


#include "UdpServerSocket.h"
#include "ReliableUdpConnection.h"


#include "Callbacks.h"


namespace muduo {
namespace net {

class EventLoop;
class EventLoopThreadPool;

namespace rudp {

const int kConnectionsMapNum = 32;

class ReliableUdpServer : boost::noncopyable,
                          public boost::enable_shared_from_this<ReliableUdpServer> {
    public:
        typedef boost::function<void(EventLoop*)> ThreadInitCallback;
        ReliableUdpServer(EventLoop* loop,
                         const InetAddress& listenAddr,
                         const string& nameArg,
                         uint32_t serverId);
        ~ReliableUdpServer();
        const string& ipPort() const { return ipPort_; }
        const string& name() const { return name_; }
        EventLoop* getLoop() const { return loop_; }

        void setThreadNum(int numThreads);
        void setSocketNum(int socketNum);

        void setMssSize(uint16_t mssSize);
        void setMaxWindowSize(uint16_t maxWindowSize);

        void setThreadInitCallback(const ThreadInitCallback& cb) {
            threadInitCallback_ = cb;
        }
        void setMessageCallback(const MessageCallback& cb) {
            messageCallback_ = cb;
        }
        void setConnectionCallback(const ConnectionCallback& cb) {
            connectionCallback_ = cb;
        }

        void Start();
    private:

        void newConnectionInLoop(const UdpServerSocketPtr& socket, const void* message,
                std::size_t len, const InetAddress& address);
        void newConnectionInLoop(const UdpServerSocketPtr& socket, const StringPiece& message,
                const InetAddress& address);
        void removeConnection(uint64_t convId, const ReliableUdpConnectionPtr& conn);
        void removeConnectionInLoop(uint64_t convId, const ReliableUdpConnectionPtr& conn);
        void handleUdpMessage(const UdpServerSocketPtr& socket,
                Buffer* buf, Timestamp, const InetAddress& address);

        uint64_t genConvId();
        bool checkCrcchecksum(const void* buffer, std::size_t n);

        void onConnectionEstablishCallback(uint64_t convId, const InetAddress& address,
                const ReliableUdpConnectionPtr& conn, bool success);
        void onConnectionEstablishCallbackInLoop(uint64_t convId, const InetAddress& addres,
                const ReliableUdpConnectionPtr& conn, bool success);

        void onConnectionReset(uint64_t convId, const UdpServerSocketPtr& socket,
                const InetAddress& address);
        void onConnectionFlush(uint64_t convId, const UdpServerSocketPtr& socket,
                const InetAddress& address, const void* data, std::size_t len);
    private:
        typedef std::map<uint64_t, ReliableUdpConnectionPtr> ConnectionMap;
        typedef std::set<string> InConnectingIpPortSet;

        EventLoop* loop_;
        const InetAddress listenAddr_;
        const string ipPort_;
        const string name_;
        const uint32_t serverId_;
        int numSockets_;

        uint16_t mssSize_;
        uint16_t maxWindowSize_;


        boost::random::mt19937 random_;
        std::set<uint32_t> convIndexs_;


        std::vector<UdpServerSocketPtr> sockets_;
        boost::shared_ptr<EventLoopThreadPool> threadPool_;

        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;

        ThreadInitCallback threadInitCallback_;
        AtomicInt32 started_;

        mutable MutexLock mutex_[kConnectionsMapNum];
        ConnectionMap connectionsMaps_[kConnectionsMapNum];

        InConnectingIpPortSet inConnectingIpPortSet_;
};
}
}
}
