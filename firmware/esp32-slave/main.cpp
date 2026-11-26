#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_MAX31865.h>
#include "../shared/Hardware.h"
#include "../shared/Inventory.h"
#include "../shared/Integration.h"
#include "CameraSession.h"
#include "CalibrationStore.h"
#include "../shared/No2Recipe.h"
#ifndef WQ_ENABLE_CAL_PULSE
#define WQ_ENABLE_CAL_PULSE 0
#endif
using namespace wq;
Outputs outputs(SLAVE_PUMPS,8);Bus bus(Serial1);Line cli;Replay replay;CameraSession camera;CalibrationStore calibration;
bool linkSeen=false;PulseReceipt receipt;
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
void tickLoads(uint32_t now){
    if(!linkSeen||elapsed(now,lastLink,LINK_LEASE_MS)){receipt.interrupt();outputs.stop();return;}
    const bool was=outputs.gate.channel!=0;
    const bool due=was&&elapsed(now,outputs.gate.started,outputs.gate.duration);
    outputs.tick(now);
    if(was&&!outputs.gate.channel)receipt.finish(due);
}
void exception(uint8_t fn,uint8_t code){uint8_t r[5]={UNIT,uint8_t(fn|0x80),code};bus.send(r,seal(r,3));}
void handle(const uint8_t* b,size_t n){
    if(!valid(b,n)||b[0]!=UNIT)return;
    if(b[1]==3){
        if(n!=8||word(b+2)!=0||word(b+4)!=8){exception(3,2);return;}
        tickLoads(millis());lastLink=millis();linkSeen=true;
        uint8_t r[21]={UNIT,3,16};
        put(r+3,camera.gate.state(outputs.gate));
        put(r+5,outputs.gate.channel);put(r+7,temperature);put(r+9,replay.seen?replay.seq:0);
        put(r+11,calibration.mask());put(r+13,DOSING_PROTOCOL);
        put(r+15,receipt.sequence);put(r+17,receipt.result);
        bus.send(r,seal(r,19));return;
    }
    if(b[1]!=16){exception(b[1],1);return;}
    if(n!=15||word(b+2)!=16||word(b+4)!=3||b[6]!=6){exception(16,3);return;}
    uint16_t seq=word(b+7),ch=word(b+9),ms=word(b+11);
    uint32_t now=millis();tickLoads(now);
    // STOP always wins, even if sequence reused.
    bool ok=false;
    if(ch==0&&ms==0){receipt.interrupt();outputs.stop();camera.cancel();ok=true;}
    else if(replay.identical(seq,ch,ms)){ok=true;} // acknowledge without re-execution
    else if(!nextSequence(replay,seq)){exception(16,3);return;}
    else if(ch==CMD_CAPTURE&&ms==0&&linkSeen&&!elapsed(now,lastLink,LINK_LEASE_MS)){
        ok=camera.request(seq?seq:65536,outputs.gate.armed);
    }
    else if(ch==CMD_ARM&&ms==0&&linkSeen&&!outputs.gate.channel&&camera.gate.canArm()&&!elapsed(now,lastLink,LINK_LEASE_MS)){ok=outputs.arm(now);}
    else if(linkSeen&&camera.gate.canArm()&&!elapsed(now,lastLink,LINK_LEASE_MS)){
        if(ch>=1&&ch<=7&&WQ_ENABLE_CAL_PULSE&&ms>=CAL_MIN_MS&&ms<=MAX_PULSE_MS){
            ok=outputs.pulse(ch,ms,7,now);if(ok)receipt.begin(seq);
        }else if(ch>VOLUME_COMMAND_BASE&&ch<=VOLUME_COMMAND_BASE+7){
            const uint8_t pump=uint8_t(ch-VOLUME_COMMAND_BASE);uint16_t duration=0;
            if(calibration.duration(pump,ms,duration))ok=outputs.pulse(pump,duration,7,now);
            if(ok)receipt.begin(seq);
        }else if(ch>CHECK_VOLUME_BASE&&ch<=CHECK_VOLUME_BASE+7&&!outputs.gate.armed){
            uint16_t duration=0;ok=calibration.duration(uint8_t(ch-CHECK_VOLUME_BASE),ms,duration);
        }
    }
    if(!ok){exception(16,3);return;}
    replay.save(seq,ch,ms);lastLink=now;
    uint8_t r[8];memcpy(r,b,6);bus.send(r,seal(r,6));
}
void setup(){
    outputs.begin();Serial.begin(115200);calibration.begin();bus.begin(9,8);
    SPI.begin(12,13,11,10);
    if(WQ_ENABLE_PT100){
        rtdReady=rtd.begin(WQ_PT100_WIRES==3?MAX31865_3WIRE:WQ_PT100_WIRES==4?MAX31865_4WIRE:MAX31865_2WIRE);
        if(rtdReady)rtd.enable50Hz(true);
    }
    Serial.println("Slave V2: remote ARM/calibration pulse/volume/capture, local status | stop | inventory | capture ID | cal list");
    inventory(false,true);
}
void loop(){
    uint32_t now=millis();tickLoads(now);
    uint8_t buf[64];size_t n=bus.receive(buf);if(n)handle(buf,n);
    // MAX31865 library conversion blocks ~75ms. Never sample during active dosing;
    // this avoids delaying OFF timing. Publish invalid if old temperature too stale.
    if(rtdReady&&!outputs.gate.channel&&camera.gate.canArm()&&elapsed(now,lastSample,1000)){
        lastSample=now;float c=rtd.temperature(100.0f,WQ_PT100_RREF);uint8_t fault=rtd.readFault();
        temperature=(!fault&&isfinite(c)&&c>=-10&&c<=60)?uint16_t(int16_t(lroundf(c*100))):INVALID_TEMP;
        if(fault)rtd.clearFault();
    }
    if(elapsed(now,lastSample,3000))temperature=INVALID_TEMP;
    const char* s=cli.read();
    if(s){
        if(!strcmp(s,"stop")){receipt.interrupt();outputs.stop();camera.cancel();Serial.println("STOP");}
        else if(!strcmp(s,"status"))Serial.printf("armed=%d ch=%u temp_raw=%u camera_state=%u\n",outputs.gate.armed,outputs.gate.channel,temperature,camera.gate.state(outputs.gate));
        else if(!strcmp(s,"inventory")){
            if(!outputs.gate.armed&&!camera.gate.busy())inventory(false,true);else Serial.println("REJECT busy");
        }else if(!strncmp(s,"cal ",4)){
            if(outputs.gate.armed||!camera.gate.canArm())Serial.println("CAL ERROR busy");
            else if(!calibration.handle(s))Serial.println("CAL ERROR syntax");
        }else{
            unsigned long id=0;char extra;
            if(strncmp(s,"capture ",8)==0&&sscanf(s,"capture %lu %c",&id,&extra)==1&&id&&id<=UINT32_MAX){
                if(camera.request(uint32_t(id),outputs.gate.armed))Serial.printf("CAM WAIT %lu\n",id);
                else Serial.printf("CAM ERROR %lu not_ready_or_busy\n",id);
            }else Serial.println("Unknown command");
        }
    }
    if(!outputs.gate.armed)camera.tick();
}
