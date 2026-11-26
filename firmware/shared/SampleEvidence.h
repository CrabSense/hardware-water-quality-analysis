#pragma once
#include "Bench.h"
#include <math.h>
namespace wq {
// All times refer to local boot uptime, never wall clock.
// Temperature timestamp is observation of a valid Modbus register, not conversion time.
constexpr uint32_t SAMPLE_OBSERVATION_MAX_MS=1000;
constexpr uint32_t TEMP_REGISTER_MAX_AGE_MS=3000; // slave's existing invalidation limit
struct SampleEvidence {
    bool recorded=false,phValid=false,tempValid=false;
    uint32_t sampleMs=0,phObservedMs=0,tempObservedMs=0;
    float ph=0;
    int16_t tempCenti=0;
    void record(uint32_t sample,uint32_t phAt,float p,bool calibrated,
                uint32_t tempAt,int16_t t,bool pollOk){
        recorded=true;sampleMs=sample;phObservedMs=phAt;tempObservedMs=tempAt;
        phValid=calibrated&&isfinite(p)&&p>=0&&p<=14&&uint32_t(phAt-sample)<=SAMPLE_OBSERVATION_MAX_MS;
        tempValid=pollOk&&uint16_t(t)!=INVALID_TEMP&&t>=-1000&&t<=6000
            &&uint32_t(tempAt-sample)<=SAMPLE_OBSERVATION_MAX_MS;
        ph=phValid?p:0;tempCenti=tempValid?t:0;
    }
};
}
