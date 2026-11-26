#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "../shared/Hardware.h"
#include "../shared/Inventory.h"
#include "../shared/Integration.h"
#include "../shared/DosingCalibration.h"
#include "../shared/No2Recipe.h"
#ifndef WQ_ENABLE_NO2_SUPERVISED
#define WQ_ENABLE_NO2_SUPERVISED 0
#endif
#ifndef WQ_ENABLE_CAL_PULSE
#define WQ_ENABLE_CAL_PULSE 0
#endif
using namespace wq;
Outputs outputs(EITEK_RELAYS,8); Bus bus(Serial1); Line cli;
Adafruit_ADS1115 ads; bool adcReady=false;
uint32_t lastPoll=0,lastSample=0,lastGood=0;
bool linked=false; uint16_t seq=0,remoteState=0,remoteChannel=0,calMask=0,receiptSeq=0,receiptResult=0;
No2Recipe no2;No2Profile no2Profile;
bool no2Issued=false;uint16_t no2Seq=0;uint32_t imageId=0,issuedAt=0;
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
    uint8_t req[8]={UNIT,3,0,0,0,8};uint8_t res[64];size_t n=0;
    if(!transact(req,seal(req,6),res,n)||n!=21||res[2]!=16||word(res+13)!=DOSING_PROTOCOL){linked=false;calMask=0;return false;}
    receiptSeq=word(res+15);receiptResult=word(res+17);calMask=word(res+11)&0x7f;remoteState=word(res+3);remoteChannel=word(res+5);remoteTemp=static_cast<int16_t>(word(res+7));
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

// Supervised NO2: one actuator at a time; no unverified hydraulic or optical assumptions.
void no2Fault(const char* reason){
    outputs.stop();no2.fault(millis());no2Issued=false;
    stopAll();Serial.printf("NO2 FAULT %s; inspect and recover manually\n",reason);
}
void no2MoveLog(){
    no2Issued=false;Serial.printf("NO2 STEP %s\n",stepName(no2.step));
    if(no2.step==No2Step::ConfirmSample)Serial.println("Verify isolated vial contains 5 mL; then no2 confirm");
    if(no2.step==No2Step::Mix1||no2.step==No2Step::Mix2)Serial.println("Mix sample manually; then no2 confirm");
    if(no2.step==No2Step::Reaction)Serial.println("300 s reaction; ensure independent validated lighting before image");
    if(no2.step==No2Step::Complete){stopAll();Serial.println("NO2 COMPLETE supervised sequence only; concentration not computed");}
}
bool no2RemoteIdle(){
    return linked&&remoteChannel==0&&remoteState==0;
}
bool no2Preflight(){
    if(!WQ_ENABLE_NO2_SUPERVISED||!WQ_ENABLE_OUTPUTS||!WQ_ENABLE_RS485||!no2Profile.valid()||outputs.gate.armed||!no2RemoteIdle()||(calMask&3)!=3)return false;
    // Query exact requested volumes without energising any outputs, before filling.
    if(!command(CHECK_VOLUME_BASE+1,uint16_t(no2Profile.r1Ul)))return false;
    return command(CHECK_VOLUME_BASE+2,uint16_t(no2Profile.r2Ul));
}
void no2Tick(){
    if(!no2.active())return;
    uint32_t now=millis();
    if(no2.step!=No2Step::Image&&(!linked||elapsed(now,lastGood,LINK_LEASE_MS))){no2Fault("link");return;}
    if(no2.confirmationExpired(now)){no2Fault("operator_timeout");return;}
    switch(no2.step){
    case No2Step::Source:case No2Step::Sample:case No2Step::Drain:case No2Step::TotalDrain:{
        const uint8_t channel=no2.step==No2Step::Source?1:no2.step==No2Step::Sample?2:no2.step==No2Step::Drain?3:7;
        const uint32_t duration=channel==1?no2.profile.sourceMs:channel==2?no2.profile.sampleMs:channel==3?no2.profile.drainMs:no2.profile.totalDrainMs;
        if(!no2Issued){
            if(!no2RemoteIdle()||outputs.gate.armed){no2Fault("local_interlock");return;}
            if(!outputs.arm(now)||!outputs.pulse(channel,uint16_t(duration),8,now)){no2Fault("local_pulse");return;}
            issuedAt=now;no2Issued=true;
        }else if(!outputs.gate.channel){
            if(!outputs.gate.armed||!elapsed(now,issuedAt,duration)){no2Fault("local_interrupted");return;}
            outputs.stop();no2.completePulse(now);no2MoveLog();
        }
        break;}
    case No2Step::Reagent1:case No2Step::Reagent2:{
        const uint8_t pump=no2.step==No2Step::Reagent1?1:2;
        const uint16_t ul=uint16_t(pump==1?no2.profile.r1Ul:no2.profile.r2Ul);
        if(!no2Issued){
            if(!no2RemoteIdle()||outputs.gate.armed||!command(CMD_ARM,0)||!command(VOLUME_COMMAND_BASE+pump,ul)){
                no2Fault("dose_rejected");return;
            }
            no2Seq=seq;issuedAt=millis();no2Issued=true;
        }else if(receiptSeq==no2Seq&&receiptResult==3){no2Fault("dose_interrupted");}
        else if(receiptSeq==no2Seq&&receiptResult==2&&remoteChannel==0){
            if(!command(0,0)){no2Fault("dose_disarm");return;}
            remoteState=0;no2.completePulse(millis());no2MoveLog();
        }else if(elapsed(now,issuedAt,MAX_PULSE_MS+1500)){no2Fault("dose_timeout");}
        break;}
    case No2Step::Reaction:
        if(!no2RemoteIdle()||outputs.gate.armed){no2Fault("reaction_interlock");return;}
        if(no2.advanceReaction(now))no2MoveLog();
        break;
    case No2Step::Image:
        if(elapsed(now,no2.since,NO2_IMAGE_MS)){no2Fault("image_timeout");return;}
        if(!no2Issued){
            if(!no2RemoteIdle()||outputs.gate.armed||!command(CMD_CAPTURE,0)){no2Fault("capture_rejected");return;}
            imageId=seq?seq:65536u;no2Issued=true;remoteState=STATE_CAMERA_BUSY;
            Serial.printf("NO2 IMAGE %lu; verify decoded JPEG on slave UART, then no2 image %lu\n",(unsigned long)imageId,(unsigned long)imageId);
        }else if(linked&&remoteState==STATE_CAMERA_FAULT){no2Fault("camera_fault");}
        // USB init/teardown may temporarily prevent polling. No load is armed;
        // image confirmation requires a fresh link and disarmed remote state.
        break;
    default:break;
    }
}
bool no2Cli(const char* s){
    uint32_t v[6]={};
    if(!strcmp(s,"no2 status")){
        Serial.printf("NO2 %s profile=%u image_id=%lu\n",stepName(no2.step),unsigned(no2Profile.valid()),(unsigned long)imageId);return true;
    }
    if(!strcmp(s,"no2 reset")){
        if(no2.active()||outputs.gate.armed||!no2RemoteIdle())Serial.println("REJECT busy_or_link");
        else{no2.reset(millis());no2Profile=No2Profile{};imageId=0;Serial.println("NO2 RESET; inspect and clean before configuring again");}return true;
    }
    if(parseNumbers(s,"no2 profile",v,6)){
        if(no2.step!=No2Step::Idle||outputs.gate.armed||!no2RemoteIdle()){Serial.println("REJECT busy_or_link");return true;}
        No2Profile p;p.sourceMs=v[0];p.sampleMs=v[1];p.r1Ul=v[2];p.r2Ul=v[3];p.drainMs=v[4];p.totalDrainMs=v[5];
        if(!p.valid()){no2Profile=No2Profile{};Serial.println("REJECT profile");}
        else{no2Profile=p;Serial.println("NO2 PROFILE staged RAM only; verify actual sample, drop volumes and drain timings");}
        return true;
    }
    if(!strcmp(s,"no2 start")){
        if(no2.step!=No2Step::Idle||!no2Preflight()||!no2.start(no2Profile,millis())){Serial.println("REJECT NO2 disabled/profile/calibration/interlock");return true;}
        imageId=0;no2MoveLog();return true;
    }
    if(!strcmp(s,"no2 confirm")){
        if(!no2RemoteIdle()||outputs.gate.armed||!no2.confirm(millis()))Serial.println("REJECT confirmation");
        else no2MoveLog();
        return true;
    }
    if(parseNumbers(s,"no2 image",v,1)){
        if(!no2Issued||v[0]!=imageId||!no2RemoteIdle()||outputs.gate.armed||!no2.imageConfirmed(millis()))Serial.println("REJECT image confirmation");
        else no2MoveLog();
        return true;
    }
    return false;
}

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
    Serial.printf(" cal_mask=0x%02x all_calibrated=%u\n",unsigned(calMask),unsigned(calMask==0x7f));
}
void setup(){
    outputs.begin();Serial.begin(115200);Wire.begin(9,8);Wire.setTimeOut(25);
    adcReady=ads.begin(WQ_ADS_ADDRESS,&Wire);if(adcReady)ads.setGain(GAIN_TWOTHIRDS);
    bus.begin(18,17);
    Serial.println("EITEK bench V2: status | inventory | arm | pulse CH MS | calpulse CH MS | volume CH UL | capture | stop");
    inventory(true,false);
    Serial.println("NO2 supervised only: no2 profile/source/sample/r1/r2/drain/total (see docs), no2 start | confirm | image ID | reset | status");
}
void loop(){
    uint32_t now=millis();outputs.tick(now);
    if(elapsed(now,lastPoll,250)){lastPoll=now;bool ok=poll();if(!ok&&WQ_ENABLE_RS485)outputs.stop();}
    if(!no2.active()&&!outputs.gate.channel&&remoteChannel==0&&elapsed(now,lastSample,1000)){lastSample=now;
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
    no2Tick();
    const char* s=cli.read();if(!s)return;
    unsigned ch=0,ms=0;uint32_t args[2]={};
    if(!strcmp(s,"stop")){if(no2.active())no2Fault("operator_stop");else stopAll();Serial.println("STOP");}
    else if(no2Cli(s))return;
    else if(no2.active()||no2.step==No2Step::Fault){Serial.println("REJECT NO2 owns controls or fault; use no2 status/reset");return;}
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
    }else if(parseNumbers(s,"calpulse",args,2)||parseNumbers(s,"volume",args,2)){
        outputs.tick(millis());const bool raw=!strncmp(s,"calpulse ",9);
        const uint32_t pump=args[0],amount=args[1];
        bool ok=pump>=1&&pump<=7&&amount>0&&linked&&outputs.gate.armed&&!outputs.gate.channel&&remoteState==1&&remoteChannel==0;
        if(ok){
            if(raw)ok=WQ_ENABLE_CAL_PULSE&&amount>=CAL_MIN_MS&&amount<=MAX_PULSE_MS;
            else ok=amount<=CAL_MAX_UL&&(calMask&(1u<<(pump-1)));
        }
        if(ok){ok=command(uint16_t(raw?pump:VOLUME_COMMAND_BASE+pump),uint16_t(amount));
            if(ok){remoteState=2;remoteChannel=uint16_t(pump);}else stopAll();}
        Serial.println(ok?"ACCEPTED not measured":"REJECT");
    }else Serial.println("Unknown command");
}
