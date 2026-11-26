#include "../firmware/shared/Nh3Recipe.h"
#include "../firmware/shared/AssayOwnership.h"
#include <cassert>
using namespace wq;
int main(){
    Nh3Profile p;Nh3Recipe r;
    assert(!r.start(p,0));
    p.sourceMs=1000;p.sampleMs=1200;p.r1Ul=250;p.r2Ul=260;p.drainMs=1000;p.totalDrainMs=1200;
    assert(r.start(p,0));assert(!r.start(p,0));
    assert(nh3LocalChannel(r.step)==1);assert(!r.confirm(0));
    assert(r.completePulse(1000));assert(nh3LocalChannel(r.step)==2);
    assert(r.completePulse(2200));assert(r.step==Nh3Step::ConfirmSample);
    assert(r.confirm(2300));assert(nh3Pump(r.step)==6);
    assert(r.completePulse(3000));assert(r.step==Nh3Step::Mix1);assert(r.confirm(3100));assert(nh3Pump(r.step)==7);
    assert(r.completePulse(4000));assert(r.step==Nh3Step::Mix2);assert(r.confirm(4100));
    assert(r.step==Nh3Step::Reaction);assert(nh3LocalChannel(r.step)==0&&nh3Pump(r.step)==0);
    assert(!r.advanceReaction(604099));assert(r.advanceReaction(604100));
    assert(!r.completePulse(604100));assert(r.imageConfirmed(605000));assert(nh3LocalChannel(r.step)==5);
    assert(r.completePulse(606000));assert(nh3LocalChannel(r.step)==7);
    assert(r.completePulse(607200));assert(r.step==Nh3Step::Complete);
    No2Recipe no2;No3Recipe no3;
    assert(assayBlocked(AssayId::None,no2,no3,r));
    assert(assayBlocked(AssayId::No2,no2,no3,r));
    assert(assayBlocked(AssayId::No3,no2,no3,r));
    assert(!assayBlocked(AssayId::Nh3,no2,no3,r));
    r.reset(0);assert(!assayBlocked(AssayId::None,no2,no3,r));
    assert(r.start(p,0));assert(assayBlocked(AssayId::No2,no2,no3,r));
    r.move(Nh3Step::Mix1,10);assert(r.confirmationExpired(120010));assert(!r.confirm(120010));
    r.fault(120010);assert(assayBlocked(AssayId::No3,no2,no3,r));assert(!r.start(p,0));r.reset(0);
    no2.move(No2Step::Reaction,0);assert(assayBlocked(AssayId::Nh3,no2,no3,r));no2.reset(0);assert(no2.active());no2.fault(1);assert(assayBlocked(AssayId::Nh3,no2,no3,r));no2.reset(2);
    no3.move(No3Step::Complete,0);assert(assayBlocked(AssayId::Nh3,no2,no3,r));no3.reset(1);assert(!assayBlocked(AssayId::None,no2,no3,r));
    r.move(Nh3Step::Reaction,UINT32_MAX-100);assert(!r.advanceReaction(599898));assert(r.advanceReaction(599899));
    r.move(Nh3Step::Image,0);assert(!r.imageConfirmed(NH3_IMAGE_MS));
    p.r2Ul=0;assert(!p.valid());p.r2Ul=65536;assert(!p.valid());p.r2Ul=260;p.sampleMs=UINT32_MAX;assert(!p.valid());
    assert(NH3_CAL_MASK==((1u<<5)|(1u<<6)));
    // Preserve the independent Sera timing after sharing the state machine.
    no2.move(No2Step::Reaction,0);assert(!no2.advanceReaction(299999));assert(no2.advanceReaction(300000));
}
