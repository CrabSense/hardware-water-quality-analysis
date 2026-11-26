#include "../firmware/shared/Integration.h"
#include "../firmware/shared/PortMap.h"
#include <cassert>
using namespace wq;
int main(){
    CameraGate c;Gate loads;
    assert(c.canArm());assert(!c.request(0,false,0));assert(!c.request(1,true,0));
    assert(c.request(2,false,0));assert(!c.canArm());assert(c.state(loads)==STATE_CAMERA_BUSY);
    assert(!c.request(3,false,0));c.receiving(100);assert(!c.timedOut(8099));assert(c.timedOut(8100));
    c.exporting(9000);assert(!c.canArm());assert(!c.timedOut(33999));assert(c.timedOut(34000));
    c.finish(false);assert(!c.canArm());assert(c.state(loads)==STATE_CAMERA_FAULT);assert(!c.request(4,false,0));
    CameraGate clean;assert(clean.request(1,false,0));clean.finish(true);assert(clean.canArm());
    loads.arm(0);assert(clean.state(loads)==1);assert(loads.pulse(1,50,7,0));assert(clean.state(loads)==2);
    assert(!canMasterArm(true,false,STATE_CAMERA_BUSY,0));assert(!canMasterArm(true,false,STATE_CAMERA_FAULT,0));
    assert(!canMasterArm(true,true,0,0));assert(canMasterArm(true,false,0,0));
    assert(!canMasterCapture(true,true,0,0));assert(!canMasterCapture(true,false,1,0));assert(canMasterCapture(true,false,0,0));
    Replay r;assert(nextSequence(r,1));assert(!nextSequence(r,2));r.save(65535,1,1);assert(nextSequence(r,0));assert(!nextSequence(r,65534));
    assert(uniquePins(EITEK_RELAYS)&&uniquePins(SLAVE_PUMPS)&&uniquePins(EITEK_INPUTS));
    for(auto p:SLAVE_PUMPS)assert(p!=19&&p!=20&&p!=8&&p!=9&&p!=10&&p!=11&&p!=12&&p!=13&&p!=21);
    assert(EITEK_RELAYS[5]==40&&SLAVE_PUMPS[7]==18);
}
