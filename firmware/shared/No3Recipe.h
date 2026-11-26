#pragma once
#include "No2Recipe.h"
namespace wq {
constexpr uint32_t NO3_SAMPLE_UL=5000;
constexpr uint8_t NO3_LIQUID_DROPS=7;
constexpr uint32_t NO3_POWDER_MIX_MS=10000;
constexpr uint32_t NO3_REACTION_MS=120000;
constexpr uint32_t NO3_CONFIRM_MS=120000;
constexpr uint32_t NO3_IMAGE_MS=60000;
constexpr uint16_t NO3_CAL_MASK=0x1c;
enum class No3Step:uint8_t{Idle,Source,Sample,ConfirmSample,Reagent1,Mix1,Reagent2,Mix2,PowderAdd,PowderMix,Reagent3,Mix3,Reaction,Image,Drain,TotalDrain,Complete,Fault};
struct No3Profile {
    uint32_t sourceMs=0,sampleMs=0,r1Ul=0,r2Ul=0,r3Ul=0,drainMs=0,totalDrainMs=0;
    bool valid()const{
        return sourceMs>0&&sourceMs<=MAX_PULSE_MS&&sampleMs>0&&sampleMs<=MAX_PULSE_MS
            &&r1Ul>0&&r1Ul<=CAL_MAX_UL&&r2Ul>0&&r2Ul<=CAL_MAX_UL&&r3Ul>0&&r3Ul<=CAL_MAX_UL
            &&drainMs>0&&drainMs<=MAX_PULSE_MS&&totalDrainMs>0&&totalDrainMs<=MAX_PULSE_MS;
    }
};
inline const char* stepName(No3Step s){
    switch(s){
    case No3Step::Idle:return "idle";case No3Step::Source:return "source";
    case No3Step::Sample:return "sample";case No3Step::ConfirmSample:return "confirm_sample";
    case No3Step::Reagent1:return "reagent1";case No3Step::Mix1:return "mix1";
    case No3Step::Reagent2:return "reagent2";case No3Step::Mix2:return "mix2";
    case No3Step::PowderAdd:return "powder_add";case No3Step::PowderMix:return "powder_mix";
    case No3Step::Reagent3:return "reagent3";case No3Step::Mix3:return "mix3";
    case No3Step::Reaction:return "reaction";case No3Step::Image:return "image";
    case No3Step::Drain:return "drain";case No3Step::TotalDrain:return "total_drain";
    case No3Step::Complete:return "complete";case No3Step::Fault:return "fault";
    }return "unknown";
}
struct No3Recipe {
    No3Step step=No3Step::Idle;
    No3Profile profile;
    uint32_t since=0;
    bool active()const{return step!=No3Step::Idle&&step!=No3Step::Complete&&step!=No3Step::Fault;}
    void move(No3Step s,uint32_t now){step=s;since=now;}
    bool start(const No3Profile& p,uint32_t now){
        if(step!=No3Step::Idle||!p.valid())return false;
        profile=p;move(No3Step::Source,now);return true;
    }
    void fault(uint32_t now){move(No3Step::Fault,now);}
    void reset(uint32_t now){if(!active())move(No3Step::Idle,now);}
    bool completePulse(uint32_t now){
        switch(step){
        case No3Step::Source:move(No3Step::Sample,now);return true;
        case No3Step::Sample:move(No3Step::ConfirmSample,now);return true;
        case No3Step::Reagent1:move(No3Step::Mix1,now);return true;
        case No3Step::Reagent2:move(No3Step::Mix2,now);return true;
        case No3Step::Reagent3:move(No3Step::Mix3,now);return true;
        case No3Step::Drain:move(No3Step::TotalDrain,now);return true;
        case No3Step::TotalDrain:move(No3Step::Complete,now);return true;
        default:return false;
        }
    }
    bool waitingOperator()const{
        return step==No3Step::ConfirmSample||step==No3Step::Mix1||step==No3Step::Mix2
            ||step==No3Step::PowderAdd||step==No3Step::PowderMix||step==No3Step::Mix3;
    }
    bool confirm(uint32_t now){
        if(!waitingOperator()||elapsed(now,since,NO3_CONFIRM_MS))return false;
        switch(step){
        case No3Step::ConfirmSample:move(No3Step::Reagent1,now);return true;
        case No3Step::Mix1:move(No3Step::Reagent2,now);return true;
        case No3Step::Mix2:move(No3Step::PowderAdd,now);return true;
        case No3Step::PowderAdd:move(No3Step::PowderMix,now);return true;
        case No3Step::PowderMix:
            if(!elapsed(now,since,NO3_POWDER_MIX_MS))return false;
            move(No3Step::Reagent3,now);return true;
        case No3Step::Mix3:move(No3Step::Reaction,now);return true;
        default:return false;
        }
    }
    bool advanceReaction(uint32_t now){
        if(step!=No3Step::Reaction||!elapsed(now,since,NO3_REACTION_MS))return false;
        move(No3Step::Image,now);return true;
    }
    bool imageConfirmed(uint32_t now){
        if(step!=No3Step::Image||elapsed(now,since,NO3_IMAGE_MS))return false;
        move(No3Step::Drain,now);return true;
    }
    bool confirmationExpired(uint32_t now)const{return waitingOperator()&&elapsed(now,since,NO3_CONFIRM_MS);}
};
}
