#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "../shared/Hardware.h"
#include "../shared/Inventory.h"
#include "../shared/Integration.h"
using namespace wq;
Outputs outputs(EITEK_RELAYS,8); Bus bus(Serial1); Line cli;
Adafruit_ADS1115 ads; bool adcReady=false;
uint32_t lastPoll=0,lastSample=0,lastGood=0;
bool linked=false; uint16_t seq=0,remoteState=0,remoteChannel=0;
int16_t remoteTemp=static_cast<int16_t>(INVALID_TEMP);
float adc1=NAN,adc2=NAN;
#ifndef WQ_ADS_ADDRESS
#define WQ_ADS_ADDRESS 0x48
#endif
// Calibrate end-to-end: ADC pin volts to external terminal/sensor volts.
// Zero slope means UNCALIBRATED, never invent pH/TDS values.
#ifndef WQ_ADC1_SCALE
#define WQ_ADC1_SCALE 0.0f
#endif
#ifndef WQ_ADC2_SCALE
#define WQ_ADC2_SCALE 0.0f
#endif
#ifndef WQ_PH_SLOPE
#define WQ_PH_SLOPE 0.0f
#endif
#ifndef WQ_PH_OFFSET
#define WQ_PH_OFFSET 0.0f
#endif
#ifndef WQ_TDS_FACTOR
#define WQ_TDS_FACTOR 0.0f
#endif
bool transact(uint8_t* req,size_t n,uint8_t* res,size_t& rn){
    if(!WQ_ENABLE_RS485)return false;
    bus.discard();bus.send(req,n);
    uint32_t start=millis();
    while(!elapsed(millis(),start,180)){
        outputs.tick(millis());
        rn=bus.receive(res);
        if(rn&&valid(res,rn)&&res[0]==UNIT&&res[1]==req[1])return true;
        delay(1);
    }
    return false;
}
bool poll(){
    uint8_t req[8]={UNIT,3,0,0,0,4};uint8_t res[64];size_t n=0;
    if(!transact(req,seal(req,6),res,n)||n!=13||res[2]!=8){linked=false;return false;}
    remoteState=word(res+3);remoteChannel=word(res+5);remoteTemp=static_cast<int16_t>(word(res+7));
    // Fourth register is last accepted sequence. Sync on first connection.
    if(!linked)seq=word(res+9);
    linked=true;lastGood=millis();return true;
}
bool command(uint16_t ch,uint16_t ms){
    uint8_t req[15]={UNIT,16,0,16,0,3,6};
    put(req+7,++seq);put(req+9,ch);put(req+11,ms);
    uint8_t res[64];size_t n=0;
    bool ok=transact(req,seal(req,13),res,n)&&n==8&&!memcmp(req,res,6);
    if(!ok){linked=false;outputs.stop();}return ok;
}
void stopAll(){outputs.stop();if(WQ_ENABLE_RS485)command(0,0);}
void printStatus(){
    Serial.printf("link=%d local=%u armed=%d slave_state=%u slave_ch=%u temp_c=",linked,outputs.gate.channel,outputs.gate.armed,remoteState,remoteChannel);
    if(linked&&uint16_t(remoteTemp)!=INVALID_TEMP)Serial.printf("%.2f",remoteTemp/100.0f);else Serial.print("INVALID");
    Serial.printf(" ads1_pin_v=%.4f ads2_pin_v=%.4f",adc1,adc2);
    float ph=(adc1*WQ_ADC1_SCALE)*WQ_PH_SLOPE+WQ_PH_OFFSET;
    if(WQ_ADC1_SCALE>0&&WQ_PH_SLOPE!=0&&isfinite(ph)&&ph>=0&&ph<=14)Serial.printf(" ph=%.3f",ph);else Serial.print(" ph=UNCALIBRATED_OR_INVALID");
    if(WQ_ADC2_SCALE>0&&WQ_TDS_FACTOR>0&&linked&&uint16_t(remoteTemp)!=INVALID_TEMP){
        float t=remoteTemp/100.0f;float raw=adc2*WQ_ADC2_SCALE;float v=raw/(1+0.02f*(t-25));
        float ppm=(133.42f*v*v*v-255.86f*v*v+857.39f*v)*0.5f*WQ_TDS_FACTOR;
        if(isfinite(ppm)&&raw>=0&&raw<2.3f&&t>=0&&t<=50&&ppm>=0&&ppm<=1000)Serial.printf(" tds_ppm=%.1f",ppm);else Serial.print(" tds=OUT_OF_RANGE");
    }else Serial.print(" tds=UNCALIBRATED_OR_INVALID");
    Serial.println();
}
void setup(){
    outputs.begin();Serial.begin(115200);Wire.begin(9,8);Wire.setTimeOut(25);
    adcReady=ads.begin(WQ_ADS_ADDRESS,&Wire);if(adcReady)ads.setGain(GAIN_TWOTHIRDS);
    bus.begin(18,17);
    Serial.println("EITEK bench V2: status | inventory | arm | pulse CH MS | dose CH MS | capture | stop");
    inventory(true,false);
    Serial.println("No automated chemistry. Verify output polarity, ADC mode and RS485 before enabling.");
}
void loop(){
    uint32_t now=millis();outputs.tick(now);
    if(elapsed(now,lastPoll,250)){lastPoll=now;bool ok=poll();if(!ok&&WQ_ENABLE_RS485)outputs.stop();}
    if(!outputs.gate.channel&&remoteChannel==0&&elapsed(now,lastSample,1000)){lastSample=now;
        // Check bus presence before library conversion to avoid missing-device busy wait.
        if(adcReady){Wire.beginTransmission(WQ_ADS_ADDRESS);if(Wire.endTransmission()==0){
            // Nonblocking conversion polling with bounded timeout.
            auto sample=[](uint16_t mux)->float{
                ads.startADCReading(mux,false);uint32_t start=millis();
                while(!ads.conversionComplete()){outputs.tick(millis());if(elapsed(millis(),start,40))return NAN;delay(1);}
                return ads.computeVolts(ads.getLastConversionResults());
            };
            adc1=sample(ADS1X15_REG_CONFIG_MUX_SINGLE_0);adc2=sample(ADS1X15_REG_CONFIG_MUX_SINGLE_1);
        }else{adc1=adc2=NAN;}}
        printStatus();
    }
    const char* s=cli.read();if(!s)return;
    unsigned ch=0,ms=0;
    if(!strcmp(s,"stop")){stopAll();Serial.println("STOP");}
    else if(!strcmp(s,"status"))printStatus();
    else if(!strcmp(s,"inventory")){
        if(!outputs.gate.armed&&!remoteChannel&&remoteState!=STATE_CAMERA_BUSY)inventory(true,false);else Serial.println("REJECT busy");
    }else if(!strcmp(s,"capture")){
        if(!canMasterCapture(linked,outputs.gate.armed,remoteState,remoteChannel)){Serial.println("REJECT: disarm both controllers before capture");return;}
        if(command(CMD_CAPTURE,0)){remoteState=STATE_CAMERA_BUSY;Serial.printf("CAM REQUEST %u (JPEG on slave UART)\n",seq?unsigned(seq):65536u);}
        else Serial.println("REJECT: capture not acknowledged");
    }else if(!strcmp(s,"arm")){if(!WQ_ENABLE_RS485||!canMasterArm(linked,outputs.gate.channel!=0,remoteState,remoteChannel)){Serial.println("REJECT: link or remote busy/fault");return;}
        if(!outputs.arm(millis())){Serial.println("REJECT: outputs disabled");return;}
        if(!command(CMD_ARM,0))outputs.stop();else remoteState=1;Serial.println(outputs.gate.armed?"ARM request accepted":"REJECT");}
    else if(parsePulse(s,"pulse",ch,ms)){
        // Reserve OUT6 for inactive vial. No concurrent local/remote loads during bench tests.
        bool ok=ch>=1&&ch<=8&&ch!=6&&ms<=MAX_PULSE_MS&&linked&&remoteState==1&&remoteChannel==0&&outputs.pulse(ch,ms,8,millis());
        Serial.println(ok?"OK":"REJECT");
    }else if(parsePulse(s,"dose",ch,ms)){
        outputs.tick(millis());
        bool ok=ch>=1&&ch<=7&&ms>0&&ms<=MAX_PULSE_MS&&linked&&outputs.gate.armed&&!outputs.gate.channel&&remoteState==1&&remoteChannel==0;
        if(ok){ok=command(ch,ms);if(ok){remoteState=2;remoteChannel=ch;}}Serial.println(ok?"OK":"REJECT");
    }else Serial.println("Unknown command");
}
