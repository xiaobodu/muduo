
#include <stdlib.h>

#include <contrib/rudp/ReliableUdpServer.h>
#include <contrib/rudp/ReliableUdpConnection.h>
#include <contrib/rudp/cpu.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Atomic.h>
#include <muduo/base/ThreadLocal.h>


#include <boost/bind.hpp>
#include <boost/circular_buffer.hpp>

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::rudp;



class Server {
    public:
        Server(EventLoop* loop,const InetAddress& listenAddr,  uint32_t serverId, int ioThreadNum)
            : loop_(loop),
              server_(new ReliableUdpServer(loop_, listenAddr, "test", serverId)) {
             server_->setConnectionCallback(boost::bind(&Server::onConnection, this, _1));
             server_->setMessageCallback(boost::bind(&Server::onMessage, this, _1, _2));
             server_->setThreadInitCallback(boost::bind(&Server::threadInitcb, this, _1));

             if (ioThreadNum > 0) {
                 server_->setThreadNum(ioThreadNum);
             }

             if (ioThreadNum > 1) {
                 server_->setSocketNum(ioThreadNum);
             }
        }
        ~Server() {}
        void Start() {
            loop_->runEvery(kTime, boost::bind(&Server::stat, this));
            server_->Start();
        }
    private:
        void onConnection(const ReliableUdpConnectionPtr& conn) {
            if (conn->Connected()) {
                connNum_.increment();
                EntryPtr entry(new Entry(conn));
                connectionBuckets_.value().back().insert(entry);
                dumpConnectionBuckets();
                WeakEntryPtr weakEntry(entry);
                conn->setContext(weakEntry);

                LOG_INFO << "New Connetion";
            } else {
                connNum_.decrement();
                LOG_INFO << "Connection close";
            }
        }
        void onMessage(const ReliableUdpConnectionPtr& conn, Buffer* buf) {
            messageRead_.increment();
            bytesRead_.add(buf->readableBytes());
            bytesWritten_.add( buf->readableBytes());
            int mid = static_cast<int>(buf->readableBytes()) / 2;
            conn->Send(buf->peek(), mid);
            buf->retrieve(mid);
            conn->Send(buf);
            buf->retrieveAll();

            auto weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));
            EntryPtr entry(weakEntry.lock());
            if (entry) {
                connectionBuckets_.value().back().insert(entry);
                dumpConnectionBuckets();
            }
        }


        void stat() {
            LOG_INFO  << "BytesRead " << bytesRead_.get();
            LOG_INFO  << "BytesWritten " << bytesWritten_.get();
            LOG_INFO  <<  "MessageRead " << messageRead_.get();
            LOG_INFO  << static_cast<double>(messageRead_.get()) / kTime  << " PPS";
            LOG_INFO  << static_cast<double>(bytesRead_.get()) / (kTime*1024*1024)  << " MiB/s throughput";
            LOG_INFO  << "load num " << connNum_.get();
            bytesRead_.getAndSet(0);
            bytesWritten_.getAndSet(0);
            messageRead_.getAndSet(0);
        }
        void threadInitcb(EventLoop* loop) {
            connectionBuckets_.value().resize(30);
            loop->runEvery(1, boost::bind(&Server::onTimer, this));
            CoreAffinitize(cpuCounter_.getAndAdd(1));
        }


        void onTimer() {
             connectionBuckets_.value().push_back(Bucket());
             dumpConnectionBuckets();
        }

        void dumpConnectionBuckets() {
            return;
            LOG_INFO << "size = " << connectionBuckets_.value().size();
            int idx = 0;
            for (WeakConnectionList::const_iterator bucketI = connectionBuckets_.value().begin();
                    bucketI != connectionBuckets_.value().end();
                    ++bucketI, ++idx)
            {
                const Bucket& bucket = *bucketI;
                printf("[%d] len = %zd : ", idx, bucket.size());
                for (Bucket::const_iterator it = bucket.begin();
                        it != bucket.end();
                        ++it)
                {
                    bool connectionDead = (*it)->weakconn_.expired();
                    printf("%p(%ld)%s, ", get_pointer(*it), it->use_count(),
                            connectionDead ? " DEAD" : "");
                }
                puts("");
            }
        }


    private:
        typedef boost::weak_ptr<ReliableUdpConnection> WeakReliableUdpConnetionPtr;
        struct Entry {
            explicit Entry(const WeakReliableUdpConnetionPtr& weakConn)
                : weakconn_(weakConn) {
            }

            ~Entry() {
                auto conn = weakconn_.lock();
                if (conn) {
                    conn->Close();
                }
            }

            WeakReliableUdpConnetionPtr weakconn_;
        };

        typedef boost::shared_ptr<Entry> EntryPtr;
        typedef boost::weak_ptr<Entry> WeakEntryPtr;
        typedef boost::unordered_set<EntryPtr> Bucket;
        typedef boost::circular_buffer<Bucket> WeakConnectionList;

        ThreadLocal<WeakConnectionList> connectionBuckets_;
    private:
        EventLoop* loop_;
        boost::shared_ptr<ReliableUdpServer> server_;



        AtomicInt64 bytesRead_;
        AtomicInt64 messageRead_;
        AtomicInt64 bytesWritten_;
        AtomicInt64 connNum_;
        AtomicInt32 cpuCounter_;
        const double kTime = 15;
};


int main(int argc, char *argv[])
{

    muduo::Logger::setLogLevel(muduo::Logger::DEBUG);
    if (argc < 5) {
        fprintf(stderr, "Usage: sever address port serverId iothreadNUm \n");
        return 1;
    }
    uint16_t port = static_cast<uint16_t>(::atoi(argv[2]));
    InetAddress listenAddr(argv[1], port);
    uint32_t serverId = static_cast<uint32_t>(atoi(argv[3]));
    int ioThreadNum = atoi(argv[4]);

    EventLoop loop;


    Server server(&loop, listenAddr, serverId, ioThreadNum);

    server.Start();

    loop.loop();


    return 0;
}



