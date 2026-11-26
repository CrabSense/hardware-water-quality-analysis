#pragma once
#include "Bench.h"
namespace wq {
// Engineering timeouts, not validated analytical tolerances.
constexpr uint32_t BATCH_PREP_MS=90000, BATCH_DISPATCH_MS=10000, BATCH_IMAGE_MS=60000, BATCH_LIMIT_MS=1800000;
enum class BatchState:uint8_t{Idle,Running,Complete,Fault};
enum class BatchWork:uint8_t{None,Prep,Image,Drain,Total};
inline uint8_t batchBit(uint8_t id){return id>=1&&id<=3?uint8_t(1u<<(id-1)):0;}
inline uint8_t batchRequiredCal(uint8_t m){return uint8_t((m&1?3:0)|(m&2?0x1c:0)|(m&4?0x60:0));}
inline uint32_t batchReactionMs(uint8_t id){return id==1?300000u:id==2?120000u:id==3?600000u:0u;}
struct BatchPlan {
    BatchState state=BatchState::Idle; BatchWork work=BatchWork::None;
    uint8_t mask=0,started=0,reacting=0,imaged=0,drained=0,owner=0;
    uint32_t since=0,workAt=0,reactionAt[3]={};
    bool held()const{return state!=BatchState::Idle;}
    bool running()const{return state==BatchState::Running;}
    bool start(uint8_t m,bool scheduled,uint32_t now){
        if(held()||!m||m>7||(scheduled&&m!=7))return false;
        *this=BatchPlan{};mask=m;since=now;state=BatchState::Running;return true;
    }
    uint8_t nextPrep()const{
        const uint8_t order[]={3,1,2};
        for(auto id:order)if((mask&batchBit(id))&&!(started&batchBit(id)))return id;
        return 0;
    }
    uint32_t remaining(uint8_t id,uint32_t now)const{
        if(!(reacting&batchBit(id)))return UINT32_MAX;
        uint32_t age=now-reactionAt[id-1],duration=batchReactionMs(id);
        return age>=duration?0:duration-age;
    }
    uint8_t due(uint32_t now)const{
        uint8_t id=0;uint32_t late=0;
        for(uint8_t i=1;i<=3;++i)if((reacting&batchBit(i))&&remaining(i,now)==0){
            uint32_t d=uint32_t(now-reactionAt[i-1])-batchReactionMs(i);
            if(!id||d>late){id=i;late=d;}
        }return id;
    }
    bool canPrep(uint8_t id,uint32_t now)const{
        if(!running()||work!=BatchWork::None||!batchBit(id)||id!=nextPrep())return false;
        for(uint8_t i=1;i<=3;++i)if(remaining(i,now)<=BATCH_PREP_MS+BATCH_DISPATCH_MS)return false;
        return true;
    }
    bool beginPrep(uint8_t id,uint32_t now){
        if(!canPrep(id,now))return false;
        started|=batchBit(id);owner=id;work=BatchWork::Prep;workAt=now;return true;
    }
    bool endPrep(uint32_t now){
        if(work!=BatchWork::Prep||!batchBit(owner)||elapsed(now,workAt,BATCH_PREP_MS))return false;
        const uint32_t target=batchReactionMs(owner);
        for(uint8_t i=1;i<=3;++i)if(reacting&batchBit(i)){
            const uint32_t rem=remaining(i,now),gap=rem>target?rem-target:target-rem;
            if(!rem||gap<BATCH_IMAGE_MS+BATCH_DISPATCH_MS)return false;
        }
        reactionAt[owner-1]=now;reacting|=batchBit(owner);release();return true;
    }
    bool dispatchAllowed(uint32_t now)const{
        return work==BatchWork::Image&&batchBit(owner)
            &&uint32_t(now-reactionAt[owner-1])-batchReactionMs(owner)<BATCH_DISPATCH_MS;
    }
    bool beginImage(uint8_t id,uint32_t now){
        if(!running()||work!=BatchWork::None||!batchBit(id)||id!=due(now))return false;
        if(uint32_t(now-reactionAt[id-1])-batchReactionMs(id)>=BATCH_DISPATCH_MS)return false;
        owner=id;work=BatchWork::Image;workAt=now;return true;
    }
    bool endImage(){
        if(work!=BatchWork::Image||!batchBit(owner))return false;
        reacting&=uint8_t(~batchBit(owner));imaged|=batchBit(owner);release();return true;
    }
    uint8_t nextDrain()const{
        if(imaged!=mask)return 0;
        for(uint8_t i=1;i<=3;++i)if((mask&batchBit(i))&&!(drained&batchBit(i)))return i;
        return 0;
    }
    bool beginDrain(uint8_t id,uint32_t now){
        if(!running()||work!=BatchWork::None||!batchBit(id)||id!=nextDrain())return false;
        owner=id;work=BatchWork::Drain;workAt=now;return true;
    }
    bool endDrain(){
        if(work!=BatchWork::Drain||!batchBit(owner))return false;
        drained|=batchBit(owner);release();return true;
    }
    bool beginTotal(uint32_t now){
        if(!running()||work!=BatchWork::None||drained!=mask||imaged!=mask)return false;
        work=BatchWork::Total;workAt=now;return true;
    }
    bool finish(){
        if(work!=BatchWork::Total||drained!=mask)return false;
        release();state=BatchState::Complete;return true;
    }
    void release(){owner=0;work=BatchWork::None;workAt=0;}
    void fault(){release();state=BatchState::Fault;}
    bool reset(){if(running())return false;*this=BatchPlan{};return true;}
};
}
