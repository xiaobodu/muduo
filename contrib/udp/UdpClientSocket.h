#pragma once


#include <stddef.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/ptr_container/ptr_list.hpp>


#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Atomic.h>


#include "UdpSocket.h"



namespace muduo {
namespace net {

class Channel;
class EventLoop;


class UdpClientSocket;


typedef boost::shared_ptr<UdpClientSocket> UdpClientSocketPtr;

class UdpClientSocket : boost::noncopyable, public  boost::enable_shared_from_this<UdpClientSocket> {
    public:
        enum ConnectResult {
            kConnectSuccess = 0,
            kConnectFailure = 1,
            kConnectAlready = 2,
        };

        enum DisconnectResutl {
            kDisconnectSuccess = 0,
            kDisconnectFailure = 1,
        };

        typedef boost::function<void(const UdpClientSocketPtr&,Buffer*, Timestamp)> MessageCallback;
        typedef boost::function<void(const UdpClientSocketPtr&, ConnectResult)> ConnectCallback;
        typedef boost::function<void(const UdpClientSocketPtr&, DisconnectResutl)> DisconnectCallback;

        struct QueuePacket {
            QueuePacket(const void* buf, size_t len)
                : Data(new char[len]), Size(len) {
                ::memcpy(Data, buf, len);
            }

            ~QueuePacket() {
                delete[] Data;
                Data = NULL;
            }

            char* Data;
            const size_t Size;
        };

        static UdpClientSocketPtr MakeUdpClientSocket(EventLoop* loop, const string& nameArg,
                size_t maxPacketSize = KDefaultMaxPacketSize);

        ~UdpClientSocket();


        void Connect(const InetAddress& address);

        void Disconnect();

        const string& name() { return name_; }
        int PeerAddress(InetAddress* address) const {
            return socket_.peerAddress(address);
        }

        int LocalAddress(InetAddress* address) const {
            return socket_.localAddress(address);
        }

        void Write(const void* message, size_t len);

        void Write(const StringPiece& message);

        void Write(Buffer* message);

        void SetConnectCallback(const ConnectCallback& cb) {
            connectCallback_ = cb;
        }

        void SetDisconnectCallback(const DisconnectCallback& cb) {
            disconnectCallback_ = cb;
        }

        void SetMessageCallback(const MessageCallback& cb) {
            messageCallback_ = cb;
        }

        void SetReceiveBufferSize(int32_t size) {
            assert(size > 0);
            receiveBufferSize_ = size;
        }

        void SetSendBufferSize(int32_t size) {
            assert(size > 0);
            sendBufferSize_ = size;
        }
    private:
        UdpClientSocket(EventLoop* loop, const string& nameArg,
                size_t maxPacketSize = KDefaultMaxPacketSize);
        void connectInLoop(const InetAddress& address);
        void disconnectInLoop();

        bool IsWriteBlocked() const { return writeBlocked_; }
        void SetWritable() { writeBlocked_ = false; }
        void SetWriteBlocked() { writeBlocked_ = true; }

        void HandleRead(Timestamp receiveTime);
        void HandleWrite();
        ssize_t HandleWrite(QueuePacket* packet);
        void HandleError();

        void WriteInLoop(const StringPiece& message);
        void WriteInLoop(const void* message, size_t len);
    private:
        EventLoop* const loop_;
        string name_;

        UdpSocket socket_;
        int32_t sendBufferSize_;
        int32_t receiveBufferSize_;
        boost::scoped_ptr<Channel> channel_;

        const size_t maxPacketSize_;
        Buffer  readBuf_;

        MessageCallback messageCallback_;
        ConnectCallback connectCallback_;
        DisconnectCallback disconnectCallback_;

        bool writeBlocked_;
        AtomicInt32 connected;

        boost::ptr_list<QueuePacket> queuePackets_;

};


}
}
