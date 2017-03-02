#include "Coding.h"
#include <muduo/net/Buffer.h>





namespace muduo {

namespace net {

void EncodeInt8(Buffer* buf, int8_t v) {
    buf->appendInt8(v);
}

int8_t DecodeInt8(Buffer* buf) {
    return buf->readInt8();
}

void EncodeInt16(Buffer* buf, int16_t v) {
    buf->appendInt16(v);
}

int16_t DecodeInt16(Buffer* buf) {
    return buf->readInt16();
}

void EncodeInt32(Buffer* buf, int32_t v) {
    buf->appendInt32(v);
}

int32_t DecodeInt32(Buffer* buf) {
    return buf->readInt32();
}

void EncodeInt64(Buffer* buf, int64_t v) {
    buf->appendInt64(v);
}

int64_t DecodeInt64(Buffer* buf) {
    return buf->readInt64();
}

void EncodeUint8(Buffer* buf, uint8_t uv) {
    int8_t v = static_cast<int8_t>(uv);
    buf->appendInt8(v);
}

uint8_t DecodeUint8(Buffer* buf) {
    return static_cast<uint8_t>(buf->readInt8());
}

void EncodeUint16(Buffer* buf, uint16_t uv) {
    int16_t v = static_cast<int16_t>(uv);
    buf->appendInt16(v);
}

uint16_t DecodeUint16(Buffer* buf) {
    return static_cast<uint16_t>(buf->readInt16());
}

void EncodeUint32(Buffer* buf, uint32_t uv) {
    int32_t v = static_cast<int32_t>(uv);
    buf->appendInt32(v);
}

uint32_t DecodeUint32(Buffer* buf) {
    return static_cast<uint32_t>(buf->readInt32());
}

void EncodeUint64(Buffer* buf, uint64_t uv) {
    int64_t v = static_cast<int64_t>(uv);
    buf->appendInt64(v);
}

uint64_t DecodeUint64(Buffer* buf) {
    return static_cast<uint64_t>(buf->readInt64());
}

}
}
