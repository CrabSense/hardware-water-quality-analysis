#pragma once
#include "Bench.h"
#include "DosingCalibration.h"
namespace wq {
constexpr uint32_t NO2_SAMPLE_UL=5000;
constexpr uint8_t NO2_DROPS=5;
constexpr uint32_t NO2_REACTION_MS=300000;
constexpr uint32_t NO2_CONFIRM_MS=120000;
constexpr uint32_t NO2_IMAGE_MS=60000;
constexpr uint16_t CHECK_VOLUME_BASE=0x200;
enum class No2Step:uint8_t{Idle,Source,Sample,ConfirmSample,Reagent1,Mix1,Reagent2,Mix2,Reaction,Image,Drain,TotalDrain,Complete,Fault};
struct No2Profile {
    uint32_t sourceMs=0,sampleMs=0,r1Ul=0,r2Ul=0,drainMs=0,totalDrainMs=0;
    bool valid()const{
        return sourceMs>0&&sourceMs<=MAX_PULSE_MS&&sampleMs>0&&sampleMs<=MAX_PULSE_MS
            &&r1Ul>0&&r1Ul<=CAL_MAX_UL&&r2Ul>0&&r2Ul<=CAL_MAX_UL
            &&drainMs>0&&drainMs<=MAX_PULSE_MS&&totalDrainMs>0&&totalDrainMs<=MAX_PULSE_MS;
    }
};
inline const char* stepName(No2Step s){
    switch(s){
    case No2Step::Idle:return "idle";case No2Step::Source:return "source";
    case No2Step::Sample:return "sample";case No2Step::ConfirmSample:return "confirm_sample";
    case No2Step::Reagent1:return "reagent1";case No2Step::Mix1:return "mix1";
    case No2Step::Reagent2:return "reagent2";case No2Step::Mix2:return "mix2";
    case No2Step::Reaction:return "reaction";case No2Step::Image:return "image";
    case No2Step::Drain:return "drain";case No2Step::TotalDrain:return "total_drain";
    case No2Step::Complete:return "complete";case No2Step::Fault:return "fault";
    }return "unknown";
}
struct No2Recipe {
    No2Step step=No2Step::Idle;
    No2Profile profile;
    uint32_t since=0;
    bool active()const{return step!=No2Step::Idle&&step!=No2Step::Complete&&step!=No2Step::Fault;}
    void move(No2Step s,uint32_t now){step=s;since=now;}
    bool start(const No2Profile& p,uint32_t now){
        if(step!=No2Step::Idle||!p.valid())return false;
        profile=p;move(No2Step::Source,now);return true;
    }
    void fault(uint32_t now){move(No2Step::Fault,now);}
    void reset(uint32_t now){if(!active())move(No2Step::Idle,now);}
    bool completePulse(uint32_t now){
        switch(step){
        case No2Step::Source:move(No2Step::Sample,now);return true;
        case No2Step::Sample:move(No2Step::ConfirmSample,now);return true;
        case No2Step::Reagent1:move(No2Step::Mix1,now);return true;
        case No2Step::Reagent2:move(No2Step::Mix2,now);return true;
        case No2Step::Drain:move(No2Step::TotalDrain,now);return true;
        case No2Step::TotalDrain:move(No2Step::Complete,now);return true;
        default:return false;
        }
    }
    bool confirm(uint32_t now){
        if(elapsed(now,since,NO2_CONFIRM_MS))return false;
        switch(step){
        case No2Step::ConfirmSample:move(No2Step::Reagent1,now);return true;
        case No2Step::Mix1:move(No2Step::Reagent2,now);return true;
        case No2Step::Mix2:move(No2Step::Reaction,now);return true;
        default:return false;
        }
    }
    bool advanceReaction(uint32_t now){
        if(step!=No2Step::Reaction||!elapsed(now,since,NO2_REACTION_MS))return false;
        move(No2Step::Image,now);return true;
    }
    bool imageConfirmed(uint32_t now){
        if(step!=No2Step::Image||elapsed(now,since,NO2_IMAGE_MS))return false;
        move(No2Step::Drain,now);return true;
    }
    bool confirmationExpired(uint32_t now)const{
        return (step==No2Step::ConfirmSample||step==No2Step::Mix1||step==No2Step::Mix2)
            &&elapsed(now,since,NO2_CONFIRM_MS);
    }
};
// Completion is electrical-command timing only, never a flow measurement.
struct PulseReceipt {
    uint16_t sequence=0,result=0; // 0 none,1 running,2 timed complete,3 interrupted
    void begin(uint16_t seq){sequence=seq;result=1;}
    void interrupt(){if(result==1)result=3;}
    void finish(bool timed){if(result==1)result=timed?2:3;}
};
}
