#pragma once
#include "Bench.h"
namespace wq {
constexpr uint16_t CMD_CAPTURE=0xfffe;
constexpr uint16_t CMD_ARM=0xffff;
constexpr uint16_t STATE_CAMERA_BUSY=3;
constexpr uint16_t STATE_CAMERA_FAULT=4;
constexpr uint32_t CAPTURE_WAIT_MS=8000;
constexpr uint32_t EXPORT_WAIT_MS=25000;
enum class CameraPhase : uint8_t { Idle, Queued, Receiving, Exporting, Fault };
struct CameraGate {
    CameraPhase phase=CameraPhase::Idle;
    uint32_t id=0, since=0;
    bool canArm() const {return phase==CameraPhase::Idle;}
    bool busy() const {return phase!=CameraPhase::Idle&&phase!=CameraPhase::Fault;}
    bool request(uint32_t requestId,bool armed,uint32_t now){
        if(!requestId||armed||!canArm())return false;
        id=requestId;since=now;phase=CameraPhase::Queued;return true;
    }
    void receiving(uint32_t now){phase=CameraPhase::Receiving;since=now;}
    void exporting(uint32_t now){phase=CameraPhase::Exporting;since=now;}
    void finish(bool hostStopped){phase=hostStopped?CameraPhase::Idle:CameraPhase::Fault;}
    bool timedOut(uint32_t now)const{
        return (phase==CameraPhase::Receiving&&elapsed(now,since,CAPTURE_WAIT_MS))
            ||(phase==CameraPhase::Exporting&&elapsed(now,since,EXPORT_WAIT_MS));
    }
    uint16_t state(const Gate& loads)const{
        if(phase==CameraPhase::Fault)return STATE_CAMERA_FAULT;
        if(busy())return STATE_CAMERA_BUSY;
        return loads.armed?(loads.channel?2:1):0;
    }
};
inline bool canMasterArm(bool linked,bool localActive,uint16_t state,uint16_t channel){
    return linked&&!localActive&&!channel&&(state==0||state==1);
}
inline bool canMasterCapture(bool linked,bool localArmed,uint16_t state,uint16_t channel){
    return linked&&!localArmed&&state==0&&!channel;
}
inline bool nextSequence(const Replay& r,uint16_t sequence){
    return sequence==uint16_t((r.seen?r.seq:0)+1);
}
}
