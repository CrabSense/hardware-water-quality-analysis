#include "../firmware/shared/No3Recipe.h"
#include <cassert>
using namespace wq;
int main(){
    No3Profile p;No3Recipe r;assert(!r.start(p,0));
    p.sourceMs=1000;p.sampleMs=1200;p.r1Ul=350;p.r2Ul=360;p.r3Ul=370;p.drainMs=1000;p.totalDrainMs=1100;
    assert(r.start(p,0));assert(!r.start(p,0));assert(!r.confirm(0));
    assert(r.completePulse(1000));assert(r.step==No3Step::Sample);
    assert(r.completePulse(2200));assert(r.confirm(2300));assert(r.step==No3Step::Reagent1);
    assert(r.completePulse(3000));assert(r.confirm(3100));assert(r.step==No3Step::Reagent2);
    assert(r.completePulse(4000));assert(r.confirm(4100));assert(r.step==No3Step::PowderAdd);
    assert(!r.completePulse(4100));assert(r.confirm(4200));assert(r.step==No3Step::PowderMix);
    assert(!r.confirm(14199));assert(!r.completePulse(14200));assert(r.confirm(14200));assert(r.step==No3Step::Reagent3);
    assert(r.completePulse(15000));assert(r.confirm(15100));assert(r.step==No3Step::Reaction);
    assert(!r.advanceReaction(135099));assert(r.advanceReaction(135100));assert(!r.completePulse(135100));
    assert(r.imageConfirmed(136000));assert(r.completePulse(137000));assert(r.completePulse(138100));
    assert(r.step==No3Step::Complete&&!r.active());assert(!r.start(p,0));r.reset(0);assert(r.start(p,0));
    r.move(No3Step::PowderAdd,10);assert(r.confirmationExpired(120010));assert(!r.confirm(120010));
    r.fault(120010);assert(!r.active()&&!r.start(p,0));r.reset(0);
    r.move(No3Step::PowderMix,UINT32_MAX-999);assert(!r.confirm(8999));assert(r.confirm(9000));
    r.move(No3Step::Reaction,UINT32_MAX-100);assert(r.advanceReaction(119899));
    r.move(No3Step::Image,0);assert(!r.imageConfirmed(NO3_IMAGE_MS));
    p.r3Ul=0;assert(!p.valid());p.r3Ul=65536;assert(!p.valid());p.r3Ul=370;p.sampleMs=UINT32_MAX;assert(!p.valid());
    assert(NO3_CAL_MASK==((1u<<2)|(1u<<3)|(1u<<4)));
    PulseReceipt receipt;receipt.begin(1);receipt.interrupt();receipt.finish(true);assert(receipt.result==3);
}
