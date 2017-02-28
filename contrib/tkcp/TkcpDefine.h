#pragma once

#include <stdint.h>



namespace muduo {


namespace net {

enum TcpCmd {
    kTcpCmdData,
    KTcpCmdConnectionSyncS2C,
};

class TcpSegmentHead {
    public:
        uint16_t len; //sizeof(len) + sizeof(cmd) + size(data)
        int8_t  cmd;
};


enum UdpCmd{
    kUdpCmdData,
    kUdpCmdConnectionSyncC2S,
};


class UdpSegmentHead {
    public:
        uint16_t conv;
        int8_t cmd;
};

}
}
