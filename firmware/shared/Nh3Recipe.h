#pragma once
#include "No2Recipe.h"
namespace wq {
// Candidate method from retailer listing; verify original kit leaflet before operation.
constexpr uint32_t NH3_SAMPLE_UL=5000;
constexpr uint8_t NH3_DROPS=5;
constexpr uint32_t NH3_REACTION_MS=600000;
constexpr uint32_t NH3_IMAGE_MS=NO2_IMAGE_MS;
constexpr uint16_t NH3_CAL_MASK=0x60;
using Nh3Step=No2Step;
using Nh3Profile=No2Profile;
using Nh3Recipe=TwoLiquidRecipe<NH3_REACTION_MS>;
inline uint8_t nh3LocalChannel(Nh3Step s){
    switch(s){
    case Nh3Step::Source:return 1;
    case Nh3Step::Sample:return 2;
    case Nh3Step::Drain:return 5;
    case Nh3Step::TotalDrain:return 7;
    default:return 0;
    }
}
inline uint8_t nh3Pump(Nh3Step s){
    return s==Nh3Step::Reagent1?6:s==Nh3Step::Reagent2?7:0;
}
}
