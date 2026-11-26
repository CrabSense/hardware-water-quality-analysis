#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include "../shared/Hardware.h"
using namespace wq;
constexpr uint8_t PUMPS[]={4,5,6,7,15,16,17,18};
Outputs outputs(PUMPS,8);Bus bus(Serial1);Line cli;Replay replay;
Adafruit_MAX31865 rtd(10,&SPI);
uint32_t lastLink=0,lastSample=0;bool rtdReady=false;
uint16_t temperature=INVALID_TEMP;
#ifndef WQ_PT100_WIRES
#define WQ_PT100_WIRES 2
#endif
// No temperature publication until wiring/reference verified.
#ifndef WQ_ENABLE_PT100
#define WQ_ENABLE_PT100 0
#endif
#ifndef WQ_PT100_RREF
#define WQ_PT100_RREF 430.0f
#endif
void exception(uint8_t fn,uint8_t code){uint8_t r[5]={UNIT,uint8_t(fn|0x80),code};bus.send(r,seal(r,3));}
void handle(const uint8_t* b,size_t n){
    if(!valid(b,n)||b[0]!=UNIT)return;
    if(b[1]==3){
        if(n!=8||word(b+2)!=0||word(b+4)!=4){exception(3,2);return;}
        lastLink=millis();outputs.tick(lastLink);
        uint8_t r[13]={UNIT,3,8};
        put(r+3,outputs.gate.armed?(outputs.gate.channel?2:1):0);
        put(r+5,outputs.gate.channel);put(r+7,temperature);put(r+9,replay.seen?replay.seq:0);
        bus.send(r,seal(r,11));return;
    }
    if(b[1]!=16){exception(b[1],1);return;}
    if(n!=15||word(b+2)!=16||word(b+4)!=3||b[6]!=6){exception(16,3);return;}
    uint16_t seq=word(b+7),ch=word(b+9),ms=word(b+11);
    uint32_t now=millis();outputs.tick(now);
    // STOP always wins, even if sequence reused.
    bool ok=false;
    if(ch==0&&ms==0){outputs.stop();ok=true;}
    else if(replay.identical(seq,ch,ms)){ok=true;} // acknowledge without re-execution
    else if(replay.conflict(seq)){exception(16,3);return;}
    else if(ch==0xffff&&ms==0&&!elapsed(now,lastLink,LINK_LEASE_MS)){ok=outputs.arm(now);}
    else if(ch>=1&&ch<=7&&ms>0&&ms<=MAX_PULSE_MS&&!elapsed(now,lastLink,LINK_LEASE_MS)){
        ok=outputs.pulse(ch,ms,7,now);
    }
    if(!ok){exception(16,3);return;}
    replay.save(seq,ch,ms);lastLink=now;
    uint8_t r[8];memcpy(r,b,6);bus.send(r,seal(r,6));
}
void setup(){
    outputs.begin();Serial.begin(115200);bus.begin(9,8);
    SPI.begin(12,13,11,10);
    if(WQ_ENABLE_PT100){
        rtdReady=rtd.begin(WQ_PT100_WIRES==3?MAX31865_3WIRE:WQ_PT100_WIRES==4?MAX31865_4WIRE:MAX31865_2WIRE);
        if(rtdReady)rtd.enable50Hz(true);
    }
    Serial.println("Slave V2 bench: remote ARM/pulse only; local status | stop. UVC not implemented.");
}
void loop(){
    uint32_t now=millis();outputs.tick(now);
    if(elapsed(now,lastLink,LINK_LEASE_MS))outputs.stop();
    uint8_t buf[64];size_t n=bus.receive(buf);if(n)handle(buf,n);
    // MAX31865 library conversion blocks ~75ms. Never sample during active dosing;
    // this avoids delaying OFF timing. Publish invalid if old temperature too stale.
    if(rtdReady&&!outputs.gate.channel&&elapsed(now,lastSample,1000)){
        lastSample=now;float c=rtd.temperature(100.0f,WQ_PT100_RREF);uint8_t fault=rtd.readFault();
        temperature=(!fault&&isfinite(c)&&c>=-10&&c<=60)?uint16_t(int16_t(lroundf(c*100))):INVALID_TEMP;
        if(fault)rtd.clearFault();
    }
    if(elapsed(now,lastSample,3000))temperature=INVALID_TEMP;
    const char* s=cli.read();
    if(s){if(!strcmp(s,"stop")){outputs.stop();Serial.println("STOP");}
        else if(!strcmp(s,"status"))Serial.printf("armed=%d ch=%u temp_raw=%u\n",outputs.gate.armed,outputs.gate.channel,temperature);
        else Serial.println("Unknown command");}
}
