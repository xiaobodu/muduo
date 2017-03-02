#pragma once


#include <stdint.h>



namespace muduo {

namespace net {

class Buffer;

void EncodeInt8(Buffer* buf, int8_t v);
int8_t DecodeInt8(Buffer* buf);

void EncodeInt16(Buffer* buf, int16_t v);
int16_t DecodeInt16(Buffer* buf);

void EncodeInt32(Buffer* buf, int32_t v);
int32_t DecodeInt32(Buffer* buf);

void EncodeInt64(Buffer* buf, int64_t v);
int64_t DecodeInt64(Buffer* buf);

void EncodeUint8(Buffer* buf, uint8_t uv);
uint8_t DecodeUint8(Buffer* buf);

void EncodeUint16(Buffer* buf, uint16_t uv);
uint16_t DecodeUint16(Buffer* buf);

void EncodeUint32(Buffer* buf, uint32_t uv);
uint32_t DecodeUint32(Buffer* buf);

void EncodeUint64(Buffer* buf, uint64_t uv);
uint64_t DecodeUint64(Buffer* buf);

}
}
