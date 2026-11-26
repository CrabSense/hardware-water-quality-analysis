#pragma once
#include <stdint.h>
#include <stddef.h>
namespace wq {
constexpr uint8_t EITEK_RELAYS[]={20,19,21,42,41,40,39,38};
constexpr uint8_t SLAVE_PUMPS[]={4,5,6,7,15,16,17,18};
constexpr uint8_t EITEK_INPUTS[]={1,2,3,4,5,6,15,16};
constexpr const char* RELAY_IDS[]={"source-pump","sample-pump","no2-drain","no3-drain","nh3-drain","reserve-vial-drain","total-drain","light"};
constexpr const char* DOSING_IDS[]={"no2-reagent-1","no2-reagent-2","no3-reagent-1","no3-reagent-2","no3-reagent-3","nh3-reagent-1","nh3-reagent-2","reserve-dosing"};
constexpr const char* ADC_IDS[]={"ph","tds","reserve-adc-3","reserve-adc-4"};
template<size_t N> inline bool uniquePins(const uint8_t (&pins)[N]){
    for(size_t i=0;i<N;++i)for(size_t j=i+1;j<N;++j)if(pins[i]==pins[j])return false;
    return true;
}
}
