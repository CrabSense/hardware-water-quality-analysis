#include "../firmware/shared/CleanPlan.h"
#include <cassert>
using namespace wq;
int main(){
    for(uint8_t i=0;i<=7;++i)assert(decodeDirty(encodeDirty(i))==i);
    assert(decodeDirty(UINT32_MAX)==7);assert(decodeDirty(encodeDirty(0)^0x100)==7);
    assert(decodeDirty(encodeDirty(8))==7);
    CleanProfile p[3];CleanPlan c;assert(!c.start(0,p,0));assert(!c.start(8,p,0));assert(!c.start(1,p,0));
    for(auto& q:p){q.sourceMs=100;q.fillMs=200;q.settleMs=500;q.drainMs=300;q.totalMs=400;q.cycles=2;}
    assert(c.start(5,p,0));assert(c.owner==1&&c.step==CleanStep::Route&&!c.channel());assert(!c.route(3,1));assert(c.route(1,1));
    uint32_t t=1;
    const uint8_t ids[]={1,3};
    for(uint8_t id: ids){
        if(id==3)assert(c.route(3,t));
        assert(c.channel()==id+2);assert(c.duration()==300);assert(c.pulseDone(++t));
        assert(c.channel()==7);assert(c.pulseDone(++t));assert(c.cycle==1);
        for(unsigned k=1;k<=2;++k){
            assert(c.cycle==k&&c.channel()==1);assert(c.pulseDone(++t));
            assert(c.channel()==2);assert(c.pulseDone(++t));assert(!c.channel());
            assert(!c.settleDone(t+499));t+=500;assert(c.settleDone(t));
            assert(c.channel()==id+2);assert(c.pulseDone(++t));
            assert(c.channel()==7);assert(c.pulseDone(++t));
        }
        assert(c.step==CleanStep::Verify&&!c.channel());assert(!c.verify(2,t));assert(c.verify(id,++t));
    }
    assert(c.step==CleanStep::Complete&&c.verified==5&&!c.active());assert(c.reset());
    assert(c.start(2,p,0));assert(!c.route(2,CLEAN_WAIT_MS));assert(c.expired(CLEAN_WAIT_MS));
    assert(!c.reset());c.fault(CLEAN_WAIT_MS);assert(c.reset());
    p[1].cycles=6;assert(!c.start(2,p,0));p[1].cycles=1;p[1].fillMs=UINT32_MAX;assert(!c.start(2,p,0));p[1].fillMs=200;
    assert(c.start(2,p,UINT32_MAX-100));assert(c.route(2,0));assert(c.channel()==4);assert(!c.expired(0));
    c.fault(1);assert(!c.verify(2,2));assert(c.verified==0);assert(c.reset());
}
