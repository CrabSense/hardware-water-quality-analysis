#pragma once
#include "Bench.h"
namespace wq {
constexpr uint8_t ALL_VIALS=7;
constexpr uint32_t CLEAN_WAIT_MS=120000,CLEAN_LIMIT_MS=1800000,CLEAN_SETTLE_MAX_MS=60000;
constexpr uint8_t CLEAN_MAX_CYCLES=5;
inline uint8_t cleanBit(uint8_t id){return id>=1&&id<=3?uint8_t(1u<<(id-1)):0;}
inline bool validCleanMask(uint8_t m){return m>0&&m<=ALL_VIALS;}
inline uint8_t decodeDirty(uint32_t stored){
    // Versioned byte payload plus complement. Unknown/corrupt => all dirty.
    uint8_t m=uint8_t(stored&0xff);
    return m<=ALL_VIALS&&stored==(0x43510000u|(uint32_t(uint8_t(~m))<<8)|m)?m:ALL_VIALS;
}
inline uint32_t encodeDirty(uint8_t m){return 0x43510000u|(uint32_t(uint8_t(~m))<<8)|m;}
struct CleanProfile{
    uint32_t sourceMs=0,fillMs=0,settleMs=0,drainMs=0,totalMs=0,cycles=0;
    bool valid()const{
        return sourceMs>0&&sourceMs<=MAX_PULSE_MS&&fillMs>0&&fillMs<=MAX_PULSE_MS
            &&settleMs<=CLEAN_SETTLE_MAX_MS&&drainMs>0&&drainMs<=MAX_PULSE_MS
            &&totalMs>0&&totalMs<=MAX_PULSE_MS&&cycles>0&&cycles<=CLEAN_MAX_CYCLES;
    }
};
enum class CleanStep:uint8_t{Idle,Route,InitialDrain,InitialTotal,Source,Fill,Settle,Drain,Total,Verify,Complete,Fault};
inline const char* cleanStepName(CleanStep s){
    switch(s){
    case CleanStep::Idle:return "idle";case CleanStep::Route:return "route";
    case CleanStep::InitialDrain:return "initial_drain";case CleanStep::InitialTotal:return "initial_total";
    case CleanStep::Source:return "source";case CleanStep::Fill:return "fill";case CleanStep::Settle:return "settle";
    case CleanStep::Drain:return "drain";case CleanStep::Total:return "total";
    case CleanStep::Verify:return "verify";case CleanStep::Complete:return "complete";case CleanStep::Fault:return "fault";
    }return "unknown";
}
struct CleanPlan{
    CleanStep step=CleanStep::Idle;
    CleanProfile profiles[3];
    uint8_t mask=0,verified=0,owner=0,cycle=0;
    uint32_t since=0,stepAt=0;
    bool active()const{return step!=CleanStep::Idle&&step!=CleanStep::Complete&&step!=CleanStep::Fault;}
    void move(CleanStep s,uint32_t now){step=s;stepAt=now;}
    bool next(uint32_t now){
        for(uint8_t i=1;i<=3;++i)if((mask&cleanBit(i))&&!(verified&cleanBit(i))){
            owner=i;cycle=0;move(CleanStep::Route,now);return true;
        }
        owner=0;move(CleanStep::Complete,now);return false;
    }
    bool start(uint8_t m,const CleanProfile (&p)[3],uint32_t now){
        if(step!=CleanStep::Idle||!validCleanMask(m))return false;
        for(uint8_t i=0;i<3;++i)if((m&(1u<<i))&&!p[i].valid())return false;
        *this=CleanPlan{};mask=m;since=now;
        for(uint8_t i=0;i<3;++i)profiles[i]=p[i];
        return next(now);
    }
    const CleanProfile& profile()const{return profiles[owner-1];}
    uint8_t channel()const{
        if(!cleanBit(owner))return 0;
        switch(step){
        case CleanStep::InitialDrain:case CleanStep::Drain:return uint8_t(owner+2);
        case CleanStep::InitialTotal:case CleanStep::Total:return 7;
        case CleanStep::Source:return 1;case CleanStep::Fill:return 2;
        default:return 0;
        }
    }
    uint32_t duration()const{
        if(!channel())return 0;
        switch(step){
        case CleanStep::InitialDrain:case CleanStep::Drain:return profile().drainMs;
        case CleanStep::InitialTotal:case CleanStep::Total:return profile().totalMs;
        case CleanStep::Source:return profile().sourceMs;
        case CleanStep::Fill:return profile().fillMs;
        default:return 0;
        }
    }
    bool expired(uint32_t now)const{
        return active()&&(elapsed(now,since,CLEAN_LIMIT_MS)
            ||((step==CleanStep::Route||step==CleanStep::Verify)&&elapsed(now,stepAt,CLEAN_WAIT_MS)));
    }
    bool route(uint8_t id,uint32_t now){
        if(step!=CleanStep::Route||id!=owner||expired(now))return false;
        move(CleanStep::InitialDrain,now);return true;
    }
    bool pulseDone(uint32_t now){
        if(expired(now))return false;
        switch(step){
        case CleanStep::InitialDrain:move(CleanStep::InitialTotal,now);return true;
        case CleanStep::InitialTotal:cycle=1;move(CleanStep::Source,now);return true;
        case CleanStep::Source:move(CleanStep::Fill,now);return true;
        case CleanStep::Fill:move(CleanStep::Settle,now);return true;
        case CleanStep::Drain:move(CleanStep::Total,now);return true;
        case CleanStep::Total:
            if(cycle<profile().cycles){++cycle;move(CleanStep::Source,now);}
            else move(CleanStep::Verify,now);
            return true;
        default:return false;
        }
    }
    bool settleDone(uint32_t now){
        if(step!=CleanStep::Settle||expired(now)||!elapsed(now,stepAt,profile().settleMs))return false;
        move(CleanStep::Drain,now);return true;
    }
    bool verify(uint8_t id,uint32_t now){
        if(step!=CleanStep::Verify||id!=owner||expired(now))return false;
        verified|=cleanBit(id);next(now);return true;
    }
    void fault(uint32_t now){move(CleanStep::Fault,now);}
    bool reset(){if(active())return false;*this=CleanPlan{};return true;}
};
}
