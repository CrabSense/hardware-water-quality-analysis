#include "../firmware/shared/No2Recipe.h"
#include <cassert>
using namespace wq;
int main(){
    No2Recipe r;No2Profile p;assert(!r.start(p,0));
    p.sourceMs=1000;p.sampleMs=1500;p.r1Ul=250;p.r2Ul=260;p.drainMs=1000;p.totalDrainMs=1500;
    assert(p.valid());assert(r.start(p,0));assert(!r.start(p,0));
    assert(!r.confirm(0));assert(r.completePulse(1000));assert(r.step==No2Step::Sample);
    assert(r.completePulse(2500));assert(r.step==No2Step::ConfirmSample);assert(!r.completePulse(2500));
    assert(r.confirm(3000));assert(r.step==No2Step::Reagent1);
    assert(r.completePulse(4000));assert(r.step==No2Step::Mix1);assert(r.confirm(5000));
    assert(r.completePulse(6000));assert(r.step==No2Step::Mix2);assert(r.confirm(7000));
    assert(!r.advanceReaction(306999));assert(r.advanceReaction(307000));assert(!r.completePulse(307000));
    assert(r.imageConfirmed(310000));assert(r.completePulse(311000));assert(r.completePulse(312000));
    assert(r.step==No2Step::Complete&&!r.active());assert(!r.start(p,0));r.reset(0);assert(r.start(p,0));
    r.completePulse(1);r.completePulse(2);assert(r.confirmationExpired(120002));assert(!r.confirm(120002));
    r.fault(120002);assert(!r.active()&&!r.start(p,0));r.reset(0);
    p.r1Ul=0;assert(!p.valid());p.r1Ul=250;p.sampleMs=UINT32_MAX;assert(!p.valid());
    PulseReceipt q;q.begin(42);assert(q.result==1);q.interrupt();q.finish(true);assert(q.result==3&&q.sequence==42);
    q.begin(43);q.finish(true);assert(q.result==2&&q.sequence==43);
    No2Recipe wrap;wrap.move(No2Step::Reaction,UINT32_MAX-100);assert(wrap.advanceReaction(299899));
    wrap.move(No2Step::Image,0);assert(!wrap.imageConfirmed(NO2_IMAGE_MS));
}
