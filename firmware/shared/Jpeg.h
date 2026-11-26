#pragma once
#include <stddef.h>
#include <stdint.h>
namespace wq {
constexpr size_t CAMERA_MAX_BYTES=55*1024;
constexpr uint32_t CAMERA_TIMEOUT_MS=5000;
inline bool jpegEnvelope(const uint8_t* b,size_t n){
    return b&&n>=4&&n<=CAMERA_MAX_BYTES&&b[0]==0xff&&b[1]==0xd8&&b[n-2]==0xff&&b[n-1]==0xd9;
}
inline uint32_t jpegCrc(const uint8_t* b,size_t n){
    uint32_t c=0xffffffff;
    for(size_t i=0;i<n;++i){c^=b[i];for(unsigned k=0;k<8;++k)c=(c>>1)^((c&1)?0xedb88320u:0u);}
    return c^0xffffffff;
}
}
