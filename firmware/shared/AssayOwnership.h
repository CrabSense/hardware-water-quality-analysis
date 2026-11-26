#pragma once
#include "No2Recipe.h"
#include "No3Recipe.h"
#include "Nh3Recipe.h"
namespace wq {
enum class AssayId:uint8_t{None,No2,No3,Nh3};
template<class R,class S> inline bool assayHeld(const R& r,S fault,S complete){
    // Keep ownership until operator confirms recovery/cleaning via reset,
    // including after completion: shared wet path must not be reused silently.
    return r.active()||r.step==fault||r.step==complete;
}
inline bool assayBlocked(AssayId requester,const No2Recipe& no2,const No3Recipe& no3,const Nh3Recipe& nh3){
    return (requester!=AssayId::No2&&assayHeld(no2,No2Step::Fault,No2Step::Complete))
        ||(requester!=AssayId::No3&&assayHeld(no3,No3Step::Fault,No3Step::Complete))
        ||(requester!=AssayId::Nh3&&assayHeld(nh3,Nh3Step::Fault,Nh3Step::Complete));
}
}
