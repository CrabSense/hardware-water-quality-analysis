#pragma once
#include <Arduino.h>
#include "PortMap.h"
#include "Hardware.h"
namespace wq {
// Manifest declares firmware-known wiring, not detected loads or network provisioning.
// No credentials, no commands, no claims that a physical device is present.
inline void portJson(bool& first,const char* id,const char* kind,const char* direction,
                     unsigned channel,int gpio,int gpio2,bool assigned){
    if(!first)Serial.print(",");
    first=false;
    Serial.printf("{\"portId\":\"%s\",\"kind\":\"%s\",\"direction\":\"%s\",\"channel\":%u,",id,kind,direction,channel);
    if(gpio>=0)Serial.printf("\"gpio\":%d,",gpio);else Serial.print("\"gpio\":null,");
    if(gpio2>=0)Serial.printf("\"gpio2\":%d,",gpio2);
    Serial.printf("\"assigned\":%s,\"discovery\":\"declared\",\"remoteControl\":false}",assigned?"true":"false");
}
inline void inventory(bool master,bool cameraSupported){
    // Stable hardware identity, not an authentication token or provisioned deviceCode.
    Serial.printf("INVENTORY {\"schemaVersion\":1,\"revision\":2,\"board\":\"%s\",\"role\":\"%s\",\"hardwareId\":\"%012llx\",\"outputsEnabled\":%s,\"rs485Enabled\":%s,\"cameraSupported\":%s,\"ports\":[",
        master?"EITEK-WERS3-8IO-4AI":"ESP32-S3-N16R8",master?"master":"slave",
        (unsigned long long)(ESP.getEfuseMac()&0xffffffffffffULL),
        WQ_ENABLE_OUTPUTS?"true":"false",WQ_ENABLE_RS485?"true":"false",cameraSupported?"true":"false");
    bool first=true;
    if(master){
        for(unsigned i=0;i<8;++i)portJson(first,RELAY_IDS[i],i<2?"relay_pump":i==7?"relay_light":"relay_valve","output",i+1,EITEK_RELAYS[i],-1,i!=5);
        for(unsigned i=0;i<8;++i){char id[8];snprintf(id,sizeof(id),"di-%u",i+1);portJson(first,id,"digital_input","input",i+1,EITEK_INPUTS[i],-1,false);}
        for(unsigned i=0;i<4;++i)portJson(first,ADC_IDS[i],"ads1115","input",i+1,-1,-1,i<2);
        portJson(first,"rs485","uart","bidirectional",1,17,18,true);
    }else{
        for(unsigned i=0;i<8;++i)portJson(first,DOSING_IDS[i],"mosfet_pump","output",i+1,SLAVE_PUMPS[i],-1,i<7);
        portJson(first,"pt100","max31865_spi","input",1,10,-1,true);
        portJson(first,"rs485","uart","bidirectional",1,8,9,true);
        portJson(first,"camera","usb_uvc","input",1,19,20,cameraSupported);
    }
    Serial.println("]}");
}
}
