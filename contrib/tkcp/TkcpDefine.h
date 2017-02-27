#pragma once

#include <stdint.h>



namespace muduo {


namespace net {

class TcpSegmentHead {
    public:
        int32_t len; //sizeof(len) + sizeof(cmd) + size(data)
        int8_t  cmd;
};


enum UdpCmd{
    kUdpCmdData,
};


class UdpSegmentHead {
    public:
        int8_t cmd;
};

}
}
