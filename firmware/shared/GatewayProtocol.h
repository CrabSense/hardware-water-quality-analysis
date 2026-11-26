#pragma once
#include "DosingCalibration.h"
namespace wq {
inline bool gwHex(const char* s,size_t n){
    if(strlen(s)!=n)return false;
    for(size_t i=0;i<n;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    return true;
}
inline bool gwAllowed(const char* s){
    uint32_t v[7]={};
    return !strcmp(s,"batch start scheduled")||!strcmp(s,"batch reset")||!strcmp(s,"clean reset")
        ||parseNumbers(s,"batch start manual",v,1)||parseNumbers(s,"batch route",v,1)
        ||parseNumbers(s,"batch confirm",v,1)||parseNumbers(s,"batch optical",v,1)
        ||parseNumbers(s,"batch image",v,2)||parseNumbers(s,"clean start",v,1)
        ||parseNumbers(s,"clean route",v,1)||parseNumbers(s,"clean verify",v,1)
        ||parseNumbers(s,"clean profile",v,7)||parseNumbers(s,"no2 profile",v,6)
        ||parseNumbers(s,"no3 profile",v,7)||parseNumbers(s,"nh3 profile",v,6);
}
struct GatewaySequence{
    uint32_t last=0;
    bool consume(uint32_t seq){
        if(last==UINT32_MAX||seq!=last+1)return false;
        last=seq;return true;
    }
};
struct GatewayRequest{
    char boot[17]={},id[33]={},cmd[80]={};
    uint32_t seq=0;
    bool parse(const char* line){
        if(strncmp(line,"gw ",3))return false;
        char seqText[11]={};
        const char* p=line+3;const char* end=strchr(p,' ');
        if(!end||end-p!=16)return false;
        memcpy(boot,p,16);boot[16]=0;p=end+1;end=strchr(p,' ');
        if(!end||end-p<1||end-p>10)return false;
        const size_t n=size_t(end-p);memcpy(seqText,p,n);seqText[n]=0;
        uint64_t value=0;
        for(size_t i=0;i<n;++i){if(seqText[i]<'0'||seqText[i]>'9')return false;value=value*10+uint64_t(seqText[i]-'0');}
        if(!value||value>UINT32_MAX)return false;
        seq=uint32_t(value);p=end+1;end=strchr(p,' ');
        if(!end||end-p!=32)return false;
        memcpy(id,p,32);id[32]=0;p=end+1;
        if(!*p||strlen(p)>=sizeof(cmd))return false;
        strcpy(cmd,p);return gwHex(boot,16)&&gwHex(id,32);
    }
};
}
