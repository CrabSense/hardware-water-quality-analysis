#include "../firmware/shared/SampleEvidence.h"
#include <cassert>
using namespace wq;
int main(){
    SampleEvidence e;assert(!e.recorded);
    e.record(1000,1040,8.0f,true,1100,2500,true);
    assert(e.recorded&&e.phValid&&e.tempValid&&e.ph==8.0f&&e.tempCenti==2500);
    e.record(1000,1040,8.0f,false,1100,2500,false);assert(!e.phValid&&!e.tempValid);
    e.record(1000,1040,NAN,true,1100,int16_t(INVALID_TEMP),true);assert(!e.phValid&&!e.tempValid);
    e.record(1000,2001,8.0f,true,2001,2500,true);assert(!e.phValid&&!e.tempValid);
    e.record(UINT32_MAX-100,30,7.5f,true,50,2500,true);assert(e.phValid&&e.tempValid);
    e.record(100,99,7.5f,true,99,2500,true);assert(!e.phValid&&!e.tempValid);
    e.record(0,1,14.1f,true,2,6001,true);assert(!e.phValid&&!e.tempValid);
    e.record(0,1,0.0f,true,2,-1000,true);assert(e.phValid&&e.tempValid);
}
