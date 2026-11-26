#include "../firmware/shared/DosingCalibration.h"
#include <cassert>
using namespace wq;
int main(){
    Calibration c;uint16_t ms=99;assert(!c.duration(100,ms)&&ms==0);
    uint32_t a[3]={990,1000,1010},b[3]={2980,3000,3020};
    assert(fit(1000,a,3000,b,c));assert(!c.valid());assert(!c.duration(2000,ms));
    c.checkMs=2000;c.checkUl=2110;assert(!c.valid());
    c.checkUl=2020;assert(c.valid());assert(c.duration(2000,ms)&&ms==2000);
    assert(c.duration(1000,ms)&&ms==1000);assert(c.duration(3000,ms)&&ms==3000);
    assert(!c.duration(999,ms));assert(!c.duration(3001,ms));assert(!c.duration(65536,ms));
    uint32_t bad[3]={800,1000,1200};Calibration d;assert(!fit(1000,bad,3000,b,d));
    assert(!fit(1000,b,3000,a,d));assert(!fit(1000,a,1200,b,d));assert(!fit(UINT32_MAX,a,3000,b,d));
    uint8_t data[CAL_BYTES];encodeCalibration(c,7,data);assert(decodeCalibration(data,7,d)&&d.valid());
    assert(!decodeCalibration(data,6,d));data[13]^=1;assert(!decodeCalibration(data,7,d));
    uint32_t p[2];assert(parseNumbers("volume 7 250","volume",p,2)&&p[0]==7&&p[1]==250);
    assert(!parseNumbers("volume -1 250","volume",p,2));assert(!parseNumbers("volume 1 4294967296","volume",p,2));
    assert(!parseNumbers("volume 1 25.5","volume",p,2));assert(!parseNumbers("volume 1 250 3","volume",p,2));
    Gate g;g.arm(0);assert(!g.pulse(1,1000,7,29500));assert(g.pulse(1,500,7,29500));g.tick(30000);assert(!g.armed&&!g.channel);
}
