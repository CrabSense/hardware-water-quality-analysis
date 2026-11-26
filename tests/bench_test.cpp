#include "../firmware/shared/Bench.h"
#include <cassert>
using namespace wq;
int main(){
    Gate g;assert(!g.pulse(1,100,7,0));g.arm(0);
    assert(!g.pulse(8,100,7,0));assert(!g.pulse(1,10001,7,0));
    assert(g.pulse(1,100,7,0));assert(!g.pulse(2,100,7,1));
    g.tick(99);assert(g.channel==1);g.tick(100);assert(!g.channel);
    assert(g.pulse(7,500,7,29999));g.tick(30000);assert(!g.armed&&!g.channel);
    g.arm(0xfffffff0);assert(g.pulse(1,40,7,0xfffffff0));g.tick(24);assert(!g.channel);
    g.disarm();assert(!g.pulse(1,1,7,25));
    uint8_t f[8]={1,3,0,0,0,4};assert(seal(f,6)==8);assert(f[6]==0x44&&f[7]==0x9);assert(valid(f,8));f[3]^=1;assert(!valid(f,8));
    Replay r;assert(!r.conflict(1));r.save(1,2,300);assert(r.identical(1,2,300));assert(r.conflict(1)&&!r.identical(1,3,300));
    unsigned ch,ms;assert(parsePulse("dose 1 100","dose",ch,ms)&&ch==1&&ms==100);assert(!parsePulse("dose 1 100 x","dose",ch,ms));
}
