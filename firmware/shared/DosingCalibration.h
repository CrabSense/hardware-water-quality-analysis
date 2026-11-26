#pragma once
#include "Bench.h"
#include <stdint.h>
namespace wq {
constexpr uint16_t VOLUME_COMMAND_BASE=0x100;
constexpr uint16_t DOSING_PROTOCOL=3;
constexpr uint32_t CAL_SCHEMA=1;
constexpr uint32_t CAL_MIN_MS=250;
constexpr uint32_t CAL_MIN_SPAN_MS=500;
constexpr uint32_t CAL_MAX_UL=65535;
constexpr uint32_t CAL_REPEAT_PERCENT=5; // range / mean
constexpr uint32_t CAL_VERIFY_PERCENT=5;
struct Calibration {
    uint32_t loMs=0,hiMs=0,loUl=0,hiUl=0,checkMs=0,checkUl=0;
    bool fitted()const{
        // Validate bounds and ordering before subtraction; adding to untrusted
        // uint32_t loMs can wrap around and admit an invalid interval.
        return loMs>=CAL_MIN_MS&&loMs<=MAX_PULSE_MS&&hiMs<=MAX_PULSE_MS
            &&hiMs>loMs&&(hiMs-loMs)>=CAL_MIN_SPAN_MS
            &&loUl>0&&hiUl<=CAL_MAX_UL&&hiUl>loUl;
    }
    uint32_t expected(uint32_t ms)const{
        if(!fitted()||ms<loMs||ms>hiMs)return 0;
        const uint64_t span=hiMs-loMs;
        return loUl+uint32_t((uint64_t(ms-loMs)*(hiUl-loUl)+span/2)/span);
    }
    bool valid()const{
        if(!fitted()||checkMs<=loMs||checkMs>=hiMs||!checkUl||checkUl>CAL_MAX_UL)return false;
        uint32_t p=expected(checkMs);uint32_t error=p>checkUl?p-checkUl:checkUl-p;
        return p>0&&uint64_t(error)*100<=uint64_t(p)*CAL_VERIFY_PERCENT;
    }
    bool duration(uint32_t ul,uint16_t& ms)const{
        ms=0;if(!valid()||ul<loUl||ul>hiUl)return false;
        const uint64_t dv=hiUl-loUl;
        uint32_t t=loMs+uint32_t((uint64_t(ul-loUl)*(hiMs-loMs)+dv/2)/dv);
        if(t<loMs||t>hiMs||t>MAX_PULSE_MS)return false;
        // Reject targets whose millisecond rounding alone exceeds tolerance.
        uint32_t v=expected(t),e=v>ul?v-ul:ul-v;
        if(uint64_t(e)*100>uint64_t(ul)*CAL_VERIFY_PERCENT)return false;
        ms=uint16_t(t);return true;
    }
};
inline bool meanRepeat(const uint32_t (&v)[3],uint32_t& mean){
    mean=0;uint32_t lo=v[0],hi=v[0];uint64_t sum=0;
    for(auto n:v){if(!n||n>CAL_MAX_UL)return false;if(n<lo)lo=n;if(n>hi)hi=n;sum+=n;}
    if(uint64_t(hi-lo)*300>sum*CAL_REPEAT_PERCENT)return false;
    mean=uint32_t((sum+1)/3);return true;
}
inline bool fit(uint32_t loMs,const uint32_t (&lo)[3],uint32_t hiMs,const uint32_t (&hi)[3],Calibration& out){
    Calibration c;c.loMs=loMs;c.hiMs=hiMs;
    if(!meanRepeat(lo,c.loUl)||!meanRepeat(hi,c.hiUl)||!c.fitted())return false;
    out=c;return true;
}
// Stable serialized layout independent of compiler padding; per-channel CRC protects accidental corruption.
constexpr size_t CAL_BYTES=36;
inline void put32(uint8_t* p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
inline uint32_t get32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
inline void encodeCalibration(const Calibration& c,uint8_t channel,uint8_t (&b)[CAL_BYTES]){
    put32(b,CAL_SCHEMA);put32(b+4,channel);put32(b+8,c.loMs);put32(b+12,c.hiMs);
    put32(b+16,c.loUl);put32(b+20,c.hiUl);put32(b+24,c.checkMs);put32(b+28,c.checkUl);
    put32(b+32,crc(b,32));
}
inline bool decodeCalibration(const uint8_t (&b)[CAL_BYTES],uint8_t channel,Calibration& c){
    if(channel<1||channel>7||get32(b)!=CAL_SCHEMA||get32(b+4)!=channel||get32(b+32)!=crc(b,32))return false;
    Calibration candidate;candidate.loMs=get32(b+8);candidate.hiMs=get32(b+12);candidate.loUl=get32(b+16);
    candidate.hiUl=get32(b+20);candidate.checkMs=get32(b+24);candidate.checkUl=get32(b+28);
    if(!candidate.valid())return false;
    c=candidate;return true;
}
// Strict decimal token parser; reject signs, overflow, suffixes and excess arguments.
inline bool parseNumbers(const char* line,const char* prefix,uint32_t* out,size_t count){
    const size_t n=strlen(prefix);
    if(strncmp(line,prefix,n)||line[n]!=' ')return false;
    const char* p=line+n;
    for(size_t i=0;i<count;++i){
        while(*p==' ')++p;
        if(*p<'0'||*p>'9')return false;
        uint32_t v=0;
        do{uint32_t digit=uint32_t(*p-'0');if(v>(UINT32_MAX-digit)/10)return false;v=v*10+digit;++p;}while(*p>='0'&&*p<='9');
        if(*p&&*p!=' ')return false;
        out[i]=v;
    }
    while(*p==' ')++p;
    return !*p;
}
}
