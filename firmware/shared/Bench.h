#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace wq {
constexpr uint16_t MAX_PULSE_MS = 10000;
constexpr uint32_t LINK_LEASE_MS = 1500;
constexpr uint32_t ARM_LEASE_MS = 30000;
constexpr uint8_t UNIT = 1;
constexpr uint16_t INVALID_TEMP = 0x8000;
inline bool elapsed(uint32_t now,uint32_t start,uint32_t interval) {
    return uint32_t(now-start)>=interval;
}
inline uint16_t crc(const uint8_t* p,size_t n) {
    uint16_t c=0xffff;
    for(size_t i=0;i<n;++i){ c^=p[i]; for(int j=0;j<8;++j)c=(c&1)?(c>>1)^0xa001:c>>1; }
    return c;
}
inline uint16_t word(const uint8_t* p){return (uint16_t(p[0])<<8)|p[1];}
inline void put(uint8_t* p,uint16_t x){p[0]=x>>8;p[1]=x&255;}
inline size_t seal(uint8_t* p,size_t n){uint16_t c=crc(p,n);p[n]=c&255;p[n+1]=c>>8;return n+2;}
inline bool valid(const uint8_t* p,size_t n){return n>=4&&crc(p,n-2)==(uint16_t(p[n-2])|(uint16_t(p[n-1])<<8));}
struct Gate {
    bool armed=false; uint8_t channel=0; uint32_t armedAt=0, started=0; uint16_t duration=0;
    void arm(uint32_t now){stop();armed=true;armedAt=now;}
    void stop(){channel=0;duration=0;}
    void disarm(){stop();armed=false;}
    bool pulse(uint8_t ch,uint16_t ms,uint8_t limit,uint32_t now){
        tick(now);
        if(!armed||channel||ch<1||ch>limit||!ms||ms>MAX_PULSE_MS)return false;
        if(uint32_t(now-armedAt)+ms>ARM_LEASE_MS)return false;
        channel=ch;started=now;duration=ms;return true;
    }
    void tick(uint32_t now){if(armed&&elapsed(now,armedAt,ARM_LEASE_MS))disarm();
        else if(channel&&elapsed(now,started,duration))stop();}
};
struct Replay {
    bool seen=false; uint16_t seq=0,ch=0,ms=0;
    bool identical(uint16_t s,uint16_t c,uint16_t d)const{return seen&&seq==s&&ch==c&&ms==d;}
    bool conflict(uint16_t s)const{return seen&&seq==s;}
    void save(uint16_t s,uint16_t c,uint16_t d){seen=true;seq=s;ch=c;ms=d;}
};
inline bool parsePulse(const char* line,const char* prefix,unsigned& ch,unsigned& ms){
    size_t n=strlen(prefix);
    if(strncmp(line,prefix,n)||line[n]!=' ')return false;
    char extra; return sscanf(line+n+1,"%u %u %c",&ch,&ms,&extra)==2;
}
}
