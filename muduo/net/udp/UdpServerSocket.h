#pragma once

#include <stddef.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/ptr_container/ptr_list.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>


#include "UdpSocket.h"



namespace muduo {
namespace net {

class Channel;
class EventLoop;


class UdpServerSocket;

typedef boost::shared_ptr<UdpServerSocket> UdpServerSocketPtr;

class UdpServerSocket : boost::noncopyable, public boost::enable_shared_from_this<UdpServerSocket> {
    public:
        typedef boost::function<void(const UdpServerSocketPtr&, Buffer*, Timestamp,
                                     const InetAddress&)> MessageCallback;

        struct QueuePacket {
            QueuePacket(const void* buf, size_t len,
                        const InetAddress& addr)
                : Data(new char[len]), Size(len), PeerAddr(addr) {
                ::memcpy(Data, buf, len);
            }

            ~QueuePacket() {
                delete[] Data;
                Data = NULL;
            }

            char* Data;
            const size_t Size;
            const InetAddress PeerAddr;
        };

        UdpServerSocket(EventLoop* loop,
                const InetAddress& listenAddr,
                const string& nameArg,
                size_t maxPacketSize = KDefaultMaxPacketSize);
        ~UdpServerSocket();

        static UdpServerSocketPtr MakeUdpServerSocket(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg,
                size_t maxPacketSize = KDefaultMaxPacketSize);


        int LocalAddress(InetAddress* address) const {
            return socket_.localAddress(address);
        }

        const string& name() const {
            return name_;
        }

        EventLoop* getLoop() const  { return loop_; }

        void Start();

        void SendTo(const void* message, size_t len,
                    const InetAddress& address);
        void SendTo(Buffer* message, const InetAddress& address);

        void SendTo(const StringPiece& message, const InetAddress& address);

        void SetMessageCallback(const MessageCallback& cb) {
            messageCallback_ = cb;
        }

        void SetReceiveBufferSize(int32_t size) {
            assert(size > 0);
            receiveBufferSize_ = size;
        }

        void SetSendBufferSize(int32_t size) {
            assert(size > 0);
            sendBufferSize_ = 0;
        }
    private:
        // in loop execute
        void startInLoop();

        bool isWriteBlocked() const { return writeBlocked_; }
        void setWritable() { writeBlocked_ = false; }
        void setWriteBlocked() { writeBlocked_ = true; }

        void handleRead(Timestamp receiveTime);
        void handleWrite();
        ssize_t handleWrite(QueuePacket* packet);
        void handleError();


        void sendToInLoop(const StringPiece& message, const InetAddress& address);
        void sendToInLoop(const void* message, size_t len, const InetAddress& address);

    private:
        EventLoop* const loop_;
        InetAddress listenAddr_;
        string name_;


        UdpSocket socket_;
        int32_t sendBufferSize_;
        int32_t receiveBufferSize_;

        boost::scoped_ptr<Channel> channel_;

        const size_t maxPacketSize_;
        Buffer  readBuf_;

        MessageCallback messageCallback_;
        AtomicInt32 started_;

        bool writeBlocked_;

        boost::ptr_list<QueuePacket> queuePackets_;
};


}
}
