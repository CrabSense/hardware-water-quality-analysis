#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "../shared/Hardware.h"
#include "../shared/Inventory.h"
#include "../shared/Integration.h"
#include "../shared/DosingCalibration.h"
#include "../shared/No2Recipe.h"
#include "../shared/No3Recipe.h"
#include "../shared/Nh3Recipe.h"
#include "../shared/AssayOwnership.h"
#include "../shared/SampleEvidence.h"
#include "../shared/BatchPlan.h"
#include "DirtyStore.h"
#ifndef WQ_ENABLE_CLEAN_SUPERVISED
#define WQ_ENABLE_CLEAN_SUPERVISED 0
#endif
#ifndef WQ_ENABLE_BATCH_SUPERVISED
#define WQ_ENABLE_BATCH_SUPERVISED 0
#endif
#include <esp_system.h>
#ifndef WQ_ENABLE_NH3_SUPERVISED
#define WQ_ENABLE_NH3_SUPERVISED 0
#endif
#ifndef WQ_ENABLE_NO3_SUPERVISED
#define WQ_ENABLE_NO3_SUPERVISED 0
#endif
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
No3Recipe no3;No3Profile no3Profile;
bool no3Issued=false;uint16_t no3Seq=0;uint32_t no3ImageId=0,no3IssuedAt=0;
Nh3Recipe nh3;Nh3Profile nh3Profile;
bool nh3Issued=false;uint16_t nh3Seq=0;uint32_t nh3ImageId=0,nh3IssuedAt=0;
SampleEvidence nh3Evidence;char nh3SessionId[17]={};
DirtyStore dirtyStore;CleanPlan cleaning;CleanProfile cleanProfiles[3];
bool cleanIssued=false;uint32_t cleanPulseAt=0;
BatchPlan batch;char batchId[17]={};bool batchOptical=false,batchTotalIssued=false;
uint32_t batchTotalMs=0,batchTotalAt=0,routeAt=0;uint8_t routePrompt=0;
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
    if(batch.running()){Serial.printf("BATCH STEP %s 1 %s; manual confirmation: batch confirm 1\n",batchId,stepName(no2.step));return;}
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
    if(no2.step!=No2Step::Image&&no2.step!=No2Step::Reagent1&&no2.step!=No2Step::Reagent2&&!no2RemoteIdle()){no2Fault("unexpected_remote_state");return;}
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
            if(batch.running())Serial.printf("BATCH IMAGE {\"batch_id\":\"%s\",\"analyte\":\"NO2\",\"capture_id\":%lu,\"session_id\":\"%s-1\"}\n",batchId,(unsigned long)imageId,batchId);
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
        if(assayBlocked(AssayId::No2,no2,no3,nh3)||no2.active()||outputs.gate.armed||!no2RemoteIdle())Serial.println("REJECT busy_or_link");
        else{no2.reset(millis());no2Profile=No2Profile{};imageId=0;Serial.println("NO2 RESET; inspect and clean before configuring again");}return true;
    }
    if(parseNumbers(s,"no2 profile",v,6)){
        if(assayBlocked(AssayId::No2,no2,no3,nh3)||no2.step!=No2Step::Idle||outputs.gate.armed||!no2RemoteIdle()){Serial.println("REJECT busy_or_link");return true;}
        No2Profile p;p.sourceMs=v[0];p.sampleMs=v[1];p.r1Ul=v[2];p.r2Ul=v[3];p.drainMs=v[4];p.totalDrainMs=v[5];
        if(!p.valid()){no2Profile=No2Profile{};Serial.println("REJECT profile");}
        else{no2Profile=p;Serial.println("NO2 PROFILE staged RAM only; verify actual sample, drop volumes and drain timings");}
        return true;
    }
    if(!strcmp(s,"no2 start")){
        if(assayBlocked(AssayId::No2,no2,no3,nh3)||no2.step!=No2Step::Idle||!dirtyStore.clean()||!no2Preflight()||!dirtyStore.mark(1)||!no2.start(no2Profile,millis())){Serial.println("REJECT NO2 disabled/profile/calibration/interlock");return true;}
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

// Supervised NO3: one actuator at a time; no unverified hydraulic or optical assumptions.
void no3Fault(const char* reason){
    outputs.stop();no3.fault(millis());no3Issued=false;
    stopAll();Serial.printf("NO3 FAULT %s; inspect and recover manually\n",reason);
}
void no3MoveLog(){
    no3Issued=false;Serial.printf("NO3 STEP %s\n",stepName(no3.step));
    if(batch.running()){Serial.printf("BATCH STEP %s 2 %s; manual confirmation: batch confirm 2\n",batchId,stepName(no3.step));return;}
    if(no3.step==No3Step::ConfirmSample)Serial.println("Verify isolated vial contains 5 mL; then no3 confirm");
    if(no3.step==No3Step::Mix1||no3.step==No3Step::Mix2||no3.step==No3Step::Mix3)Serial.println("Mix sample manually; then no3 confirm");
    if(no3.step==No3Step::PowderAdd)Serial.println("Add 1/3 supplied scoop of powder manually; then no3 confirm to start mixing timer");
    if(no3.step==No3Step::PowderMix)Serial.println("Shake manually for 10 seconds; then no3 confirm");
    if(no3.step==No3Step::Reaction)Serial.println("120 s reaction; ensure independent validated lighting before image");
    if(no3.step==No3Step::Complete){stopAll();Serial.println("NO3 COMPLETE supervised sequence only; concentration not computed");}
}
bool no3RemoteIdle(){
    return linked&&remoteChannel==0&&remoteState==0;
}
bool no3Preflight(){
    if(!WQ_ENABLE_NO3_SUPERVISED||!WQ_ENABLE_OUTPUTS||!WQ_ENABLE_RS485||!no3Profile.valid()||outputs.gate.armed||!no3RemoteIdle()||(calMask&NO3_CAL_MASK)!=NO3_CAL_MASK)return false;
    // Query exact requested volumes without energising any outputs, before filling.
    if(!command(CHECK_VOLUME_BASE+3,uint16_t(no3Profile.r1Ul)))return false;
    if(!command(CHECK_VOLUME_BASE+4,uint16_t(no3Profile.r2Ul)))return false;
    return command(CHECK_VOLUME_BASE+5,uint16_t(no3Profile.r3Ul));
}
void no3Tick(){
    if(!no3.active())return;
    uint32_t now=millis();
    if(no3.step!=No3Step::Image&&(!linked||elapsed(now,lastGood,LINK_LEASE_MS))){no3Fault("link");return;}
    if(no3.confirmationExpired(now)){no3Fault("operator_timeout");return;}
    if(no3.step!=No3Step::Image&&no3.step!=No3Step::Reagent1&&no3.step!=No3Step::Reagent2&&no3.step!=No3Step::Reagent3&&!no3RemoteIdle()){no3Fault("unexpected_remote_state");return;}
    switch(no3.step){
    case No3Step::Source:case No3Step::Sample:case No3Step::Drain:case No3Step::TotalDrain:{
        const uint8_t channel=no3.step==No3Step::Source?1:no3.step==No3Step::Sample?2:no3.step==No3Step::Drain?4:7;
        const uint32_t duration=channel==1?no3.profile.sourceMs:channel==2?no3.profile.sampleMs:channel==4?no3.profile.drainMs:no3.profile.totalDrainMs;
        if(!no3Issued){
            if(!no3RemoteIdle()||outputs.gate.armed){no3Fault("local_interlock");return;}
            if(!outputs.arm(now)||!outputs.pulse(channel,uint16_t(duration),8,now)){no3Fault("local_pulse");return;}
            no3IssuedAt=now;no3Issued=true;
        }else if(!outputs.gate.channel){
            if(!outputs.gate.armed||!elapsed(now,no3IssuedAt,duration)){no3Fault("local_interrupted");return;}
            outputs.stop();no3.completePulse(now);no3MoveLog();
        }
        break;}
    case No3Step::Reagent1:case No3Step::Reagent2:case No3Step::Reagent3:{
        const uint8_t pump=no3.step==No3Step::Reagent1?3:no3.step==No3Step::Reagent2?4:5;
        const uint16_t ul=uint16_t(pump==3?no3.profile.r1Ul:pump==4?no3.profile.r2Ul:no3.profile.r3Ul);
        if(!no3Issued){
            if(!no3RemoteIdle()||outputs.gate.armed||!command(CMD_ARM,0)||!command(VOLUME_COMMAND_BASE+pump,ul)){
                no3Fault("dose_rejected");return;
            }
            no3Seq=seq;no3IssuedAt=millis();no3Issued=true;
        }else if(receiptSeq==no3Seq&&receiptResult==3){no3Fault("dose_interrupted");}
        else if(receiptSeq==no3Seq&&receiptResult==2&&remoteChannel==0){
            if(!command(0,0)){no3Fault("dose_disarm");return;}
            remoteState=0;no3.completePulse(millis());no3MoveLog();
        }else if(elapsed(now,no3IssuedAt,MAX_PULSE_MS+1500)){no3Fault("dose_timeout");}
        break;}
    case No3Step::Reaction:
        if(!no3RemoteIdle()||outputs.gate.armed){no3Fault("reaction_interlock");return;}
        if(no3.advanceReaction(now))no3MoveLog();
        break;
    case No3Step::Image:
        if(elapsed(now,no3.since,NO3_IMAGE_MS)){no3Fault("image_timeout");return;}
        if(!no3Issued){
            if(!no3RemoteIdle()||outputs.gate.armed||!command(CMD_CAPTURE,0)){no3Fault("capture_rejected");return;}
            no3ImageId=seq?seq:65536u;no3Issued=true;remoteState=STATE_CAMERA_BUSY;
            if(batch.running())Serial.printf("BATCH IMAGE {\"batch_id\":\"%s\",\"analyte\":\"NO3\",\"capture_id\":%lu,\"session_id\":\"%s-2\"}\n",batchId,(unsigned long)no3ImageId,batchId);
            Serial.printf("NO3 IMAGE %lu; verify decoded JPEG on slave UART, then no3 image %lu\n",(unsigned long)no3ImageId,(unsigned long)no3ImageId);
        }else if(linked&&remoteState==STATE_CAMERA_FAULT){no3Fault("camera_fault");}
        // USB init/teardown may temporarily prevent polling. No load is armed;
        // image confirmation requires a fresh link and disarmed remote state.
        break;
    default:break;
    }
}
bool no3Cli(const char* s){
    uint32_t v[7]={};
    if(!strcmp(s,"no3 status")){
        Serial.printf("NO3 %s profile=%u image_id=%lu\n",stepName(no3.step),unsigned(no3Profile.valid()),(unsigned long)no3ImageId);return true;
    }
    if(!strcmp(s,"no3 reset")){
        if(assayBlocked(AssayId::No3,no2,no3,nh3)||no3.active()||outputs.gate.armed||!no3RemoteIdle())Serial.println("REJECT busy_or_link");
        else{no3.reset(millis());no3Profile=No3Profile{};no3ImageId=0;Serial.println("NO3 RESET; inspect and clean before configuring again");}return true;
    }
    if(parseNumbers(s,"no3 profile",v,7)){
        if(assayBlocked(AssayId::No3,no2,no3,nh3)||no3.step!=No3Step::Idle||outputs.gate.armed||!no3RemoteIdle()){Serial.println("REJECT busy_or_link");return true;}
        No3Profile p;p.sourceMs=v[0];p.sampleMs=v[1];p.r1Ul=v[2];p.r2Ul=v[3];p.r3Ul=v[4];p.drainMs=v[5];p.totalDrainMs=v[6];
        if(!p.valid()){no3Profile=No3Profile{};Serial.println("REJECT profile");}
        else{no3Profile=p;Serial.println("NO3 PROFILE staged RAM only; verify actual sample, drop volumes and drain timings");}
        return true;
    }
    if(!strcmp(s,"no3 start")){
        if(assayBlocked(AssayId::No3,no2,no3,nh3)||no3.step!=No3Step::Idle||!dirtyStore.clean()||!no3Preflight()||!dirtyStore.mark(2)||!no3.start(no3Profile,millis())){Serial.println("REJECT NO3 disabled/profile/calibration/interlock");return true;}
        no3ImageId=0;no3MoveLog();return true;
    }
    if(!strcmp(s,"no3 confirm")){
        if(!no3RemoteIdle()||outputs.gate.armed||!no3.confirm(millis()))Serial.println("REJECT confirmation");
        else no3MoveLog();
        return true;
    }
    if(parseNumbers(s,"no3 image",v,1)){
        if(!no3Issued||v[0]!=no3ImageId||!no3RemoteIdle()||outputs.gate.armed||!no3.imageConfirmed(millis()))Serial.println("REJECT image confirmation");
        else no3MoveLog();
        return true;
    }
    return false;
}

bool nh3RemoteIdle();
// Snapshot once at end of sample pulse, before reagents and before operator confirmation.
// No UTC clock and no verified sensor-to-vial hydraulic synchronization at this layer.
void nh3NewSession(){
    snprintf(nh3SessionId,sizeof(nh3SessionId),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    nh3Evidence=SampleEvidence{};
}
float nh3ReadPh(){
    if(!adcReady||WQ_ADC1_SCALE<=0||WQ_PH_SLOPE==0)return NAN;
    Wire.beginTransmission(WQ_ADS_ADDRESS);if(Wire.endTransmission()!=0)return NAN;
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0,false);
    uint32_t start=millis();
    while(!ads.conversionComplete()){if(elapsed(millis(),start,40))return NAN;delay(1);}
    float v=ads.computeVolts(ads.getLastConversionResults());
    return v*WQ_ADC1_SCALE*WQ_PH_SLOPE+WQ_PH_OFFSET;
}
void nh3EmitEvidence(){
    if(!nh3Evidence.recorded)return;
    const auto& e=nh3Evidence;
    Serial.printf("NH3 SAMPLE {\"session_id\":\"%s\",\"sample_uptime_ms\":%lu,\"ph_observed_uptime_ms\":%lu,\"ph\":",
        nh3SessionId,(unsigned long)e.sampleMs,(unsigned long)e.phObservedMs);
    if(e.phValid)Serial.printf("%.3f",e.ph);else Serial.print("null");
    Serial.printf(",\"temperature_observed_uptime_ms\":%lu,\"temperature_c\":",(unsigned long)e.tempObservedMs);
    if(e.tempValid)Serial.printf("%.2f",e.tempCenti/100.0f);else Serial.print("null");
    Serial.printf(",\"temperature_register_max_age_ms\":%lu,\"sensor_vial_alignment_verified\":false,\"clock\":\"boot_uptime\",\"salinity_ppt\":null}\n",(unsigned long)TEMP_REGISTER_MAX_AGE_MS);
}
bool nh3Snapshot(uint32_t sampleAt){
    // Called only after disarming local outputs and before any reagent command.
    if(outputs.gate.armed||!nh3RemoteIdle())return false;
    float ph=nh3ReadPh();uint32_t phAt=millis();
    bool ok=poll();uint32_t tempAt=millis();
    nh3Evidence.record(sampleAt,phAt,ph,WQ_ADC1_SCALE>0&&WQ_PH_SLOPE!=0,tempAt,remoteTemp,ok);
    nh3EmitEvidence();
    return ok&&nh3RemoteIdle();
}

// Supervised NH3: one actuator at a time; no unverified hydraulic or optical assumptions.
void nh3Fault(const char* reason){
    outputs.stop();nh3.fault(millis());nh3Issued=false;
    stopAll();Serial.printf("NH3 FAULT %s; inspect and recover manually\n",reason);
}
void nh3MoveLog(){
    nh3Issued=false;Serial.printf("NH3 STEP %s\n",stepName(nh3.step));
    if(batch.running()){Serial.printf("BATCH STEP %s 3 %s; manual confirmation: batch confirm 3\n",batchId,stepName(nh3.step));return;}
    if(nh3.step==Nh3Step::ConfirmSample)Serial.println("Verify isolated vial contains 5 mL; then nh3 confirm");
    if(nh3.step==Nh3Step::Mix1||nh3.step==Nh3Step::Mix2)Serial.println("Mix sample manually; then nh3 confirm");
    if(nh3.step==Nh3Step::Reaction)Serial.println("600 s reaction; ensure independent validated lighting before image");
    if(nh3.step==Nh3Step::Complete){stopAll();Serial.println("NH3 COMPLETE supervised sequence only; concentration not computed");}
}
bool nh3RemoteIdle(){
    return linked&&remoteChannel==0&&remoteState==0;
}
bool nh3Preflight(){
    if(!WQ_ENABLE_NH3_SUPERVISED||!WQ_ENABLE_OUTPUTS||!WQ_ENABLE_RS485||!nh3Profile.valid()||outputs.gate.armed||!nh3RemoteIdle()||(calMask&NH3_CAL_MASK)!=NH3_CAL_MASK)return false;
    // Query exact requested volumes without energising any outputs, before filling.
    if(!command(CHECK_VOLUME_BASE+6,uint16_t(nh3Profile.r1Ul)))return false;
    return command(CHECK_VOLUME_BASE+7,uint16_t(nh3Profile.r2Ul));
}
void nh3Tick(){
    if(!nh3.active())return;
    uint32_t now=millis();
    if(nh3.step!=Nh3Step::Image&&(!linked||elapsed(now,lastGood,LINK_LEASE_MS))){nh3Fault("link");return;}
    if(nh3.confirmationExpired(now)){nh3Fault("operator_timeout");return;}
    if(nh3.step!=Nh3Step::Image&&nh3.step!=Nh3Step::Reagent1&&nh3.step!=Nh3Step::Reagent2&&!nh3RemoteIdle()){nh3Fault("unexpected_remote_state");return;}
    switch(nh3.step){
    case Nh3Step::Source:case Nh3Step::Sample:case Nh3Step::Drain:case Nh3Step::TotalDrain:{
        const uint8_t channel=nh3LocalChannel(nh3.step);
        const uint32_t duration=channel==1?nh3.profile.sourceMs:channel==2?nh3.profile.sampleMs:channel==5?nh3.profile.drainMs:nh3.profile.totalDrainMs;
        if(!nh3Issued){
            if(!nh3RemoteIdle()||outputs.gate.armed){nh3Fault("local_interlock");return;}
            if(!outputs.arm(now)||!outputs.pulse(channel,uint16_t(duration),8,now)){nh3Fault("local_pulse");return;}
            nh3IssuedAt=now;nh3Issued=true;
        }else if(!outputs.gate.channel){
            if(!outputs.gate.armed||!elapsed(now,nh3IssuedAt,duration)){nh3Fault("local_interrupted");return;}
            outputs.stop();
            if(nh3.step==Nh3Step::Sample&&!nh3Snapshot(now)){nh3Fault("sample_snapshot_interlock");return;}
            nh3.completePulse(millis());nh3MoveLog();
        }
        break;}
    case Nh3Step::Reagent1:case Nh3Step::Reagent2:{
        const uint8_t pump=nh3Pump(nh3.step);
        const uint16_t ul=uint16_t(pump==6?nh3.profile.r1Ul:nh3.profile.r2Ul);
        if(!nh3Issued){
            if(!nh3RemoteIdle()||outputs.gate.armed||!command(CMD_ARM,0)||!command(VOLUME_COMMAND_BASE+pump,ul)){
                nh3Fault("dose_rejected");return;
            }
            nh3Seq=seq;nh3IssuedAt=millis();nh3Issued=true;
        }else if(receiptSeq==nh3Seq&&receiptResult==3){nh3Fault("dose_interrupted");}
        else if(receiptSeq==nh3Seq&&receiptResult==2&&remoteChannel==0){
            if(!command(0,0)){nh3Fault("dose_disarm");return;}
            remoteState=0;nh3.completePulse(millis());nh3MoveLog();
        }else if(elapsed(now,nh3IssuedAt,MAX_PULSE_MS+1500)){nh3Fault("dose_timeout");}
        break;}
    case Nh3Step::Reaction:
        if(!nh3RemoteIdle()||outputs.gate.armed){nh3Fault("reaction_interlock");return;}
        if(nh3.advanceReaction(now))nh3MoveLog();
        break;
    case Nh3Step::Image:
        if(elapsed(now,nh3.since,NH3_IMAGE_MS)){nh3Fault("image_timeout");return;}
        if(!nh3Issued){
            if(!nh3RemoteIdle()||outputs.gate.armed||!command(CMD_CAPTURE,0)){nh3Fault("capture_rejected");return;}
            nh3ImageId=seq?seq:65536u;nh3Issued=true;remoteState=STATE_CAMERA_BUSY;
            if(batch.running())Serial.printf("BATCH IMAGE {\"batch_id\":\"%s\",\"analyte\":\"NH3\",\"capture_id\":%lu,\"session_id\":\"%s\"}\n",batchId,(unsigned long)nh3ImageId,nh3SessionId);
            Serial.printf("NH3 IMAGE %lu; verify decoded JPEG on slave UART, then nh3 image %lu\n",(unsigned long)nh3ImageId,(unsigned long)nh3ImageId);
            Serial.printf("NH3 IMAGE_LINK {\"session_id\":\"%s\",\"capture_id\":%lu,\"requested_uptime_ms\":%lu,\"reaction_finished_uptime_ms\":%lu}\n",
                nh3SessionId,(unsigned long)nh3ImageId,(unsigned long)millis(),(unsigned long)nh3.since);
        }else if(linked&&remoteState==STATE_CAMERA_FAULT){nh3Fault("camera_fault");}
        // USB init/teardown may temporarily prevent polling. No load is armed;
        // image confirmation requires a fresh link and disarmed remote state.
        break;
    default:break;
    }
}
bool nh3Cli(const char* s){
    uint32_t v[6]={};
    if(!strcmp(s,"nh3 evidence")){
        if(outputs.gate.channel||remoteChannel||!nh3Evidence.recorded)Serial.println("REJECT evidence_not_available_or_busy");else nh3EmitEvidence();return true;
    }
    if(!strcmp(s,"nh3 status")){
        Serial.printf("NH3 %s profile=%u image_id=%lu\n",stepName(nh3.step),unsigned(nh3Profile.valid()),(unsigned long)nh3ImageId);return true;
    }
    if(!strcmp(s,"nh3 reset")){
        if(assayBlocked(AssayId::Nh3,no2,no3,nh3)||nh3.active()||outputs.gate.armed||!nh3RemoteIdle())Serial.println("REJECT busy_or_link");
        else{nh3.reset(millis());nh3Profile=Nh3Profile{};nh3ImageId=0;nh3Evidence=SampleEvidence{};nh3SessionId[0]=0;Serial.println("NH3 RESET; inspect and clean before configuring again");}return true;
    }
    if(parseNumbers(s,"nh3 profile",v,6)){
        if(assayBlocked(AssayId::Nh3,no2,no3,nh3)||nh3.step!=Nh3Step::Idle||outputs.gate.armed||!nh3RemoteIdle()){Serial.println("REJECT busy_or_link");return true;}
        Nh3Profile p;p.sourceMs=v[0];p.sampleMs=v[1];p.r1Ul=v[2];p.r2Ul=v[3];p.drainMs=v[4];p.totalDrainMs=v[5];
        if(!p.valid()){nh3Profile=Nh3Profile{};Serial.println("REJECT profile");}
        else{nh3Profile=p;Serial.println("NH3 PROFILE staged RAM only; verify actual sample, drop volumes and drain timings");}
        return true;
    }
    if(!strcmp(s,"nh3 start")){
        if(assayBlocked(AssayId::Nh3,no2,no3,nh3)||nh3.step!=Nh3Step::Idle||!dirtyStore.clean()||!nh3Preflight()||!dirtyStore.mark(4)||!nh3.start(nh3Profile,millis())){Serial.println("REJECT NH3 disabled/profile/calibration/interlock");return true;}
        nh3ImageId=0;nh3NewSession();nh3MoveLog();return true;
    }
    if(!strcmp(s,"nh3 confirm")){
        if(!nh3RemoteIdle()||outputs.gate.armed||!nh3.confirm(millis()))Serial.println("REJECT confirmation");
        else nh3MoveLog();
        return true;
    }
    if(parseNumbers(s,"nh3 image",v,1)){
        if(!nh3Issued||v[0]!=nh3ImageId||!nh3RemoteIdle()||outputs.gate.armed||!nh3.imageConfirmed(millis()))Serial.println("REJECT image confirmation");
        else nh3MoveLog();
        return true;
    }
    return false;
}



const char* batchName(uint8_t id){return id==1?"NO2":id==2?"NO3":id==3?"NH3":"none";}
const char* batchStep(uint8_t id){return id==1?stepName(no2.step):id==2?stepName(no3.step):stepName(nh3.step);}
bool batchRecipeFault(uint8_t id){return id==1?no2.step==No2Step::Fault:id==2?no3.step==No3Step::Fault:nh3.step==Nh3Step::Fault;}
bool batchReaction(uint8_t id){return id==1?no2.step==No2Step::Reaction:id==2?no3.step==No3Step::Reaction:nh3.step==Nh3Step::Reaction;}
bool batchTotalStep(uint8_t id){return id==1?no2.step==No2Step::TotalDrain:id==2?no3.step==No3Step::TotalDrain:nh3.step==Nh3Step::TotalDrain;}
bool batchImageIssued(uint8_t id){return id==1?no2Issued:id==2?no3Issued:nh3Issued;}
uint32_t batchCaptureId(uint8_t id){return id==1?imageId:id==2?no3ImageId:nh3ImageId;}
void batchRunTick(uint8_t id){if(id==1)no2Tick();else if(id==2)no3Tick();else if(id==3)nh3Tick();}
bool batchIdleRemote(){return linked&&!elapsed(millis(),lastGood,LINK_LEASE_MS)&&remoteState==0&&!remoteChannel&&!outputs.gate.armed;}
void batchStatus(){
    Serial.printf("BATCH STATUS {\"batch_id\":\"%s\",\"state\":%u,\"mask\":%u,\"work\":%u,\"owner\":%u,\"started\":%u,\"reacting\":%u,\"imaged\":%u,\"drained\":%u,\"no2\":\"%s\",\"no3\":\"%s\",\"nh3\":\"%s\"}\n",
        batchId,unsigned(batch.state),unsigned(batch.mask),unsigned(batch.work),unsigned(batch.owner),unsigned(batch.started),unsigned(batch.reacting),unsigned(batch.imaged),unsigned(batch.drained),batchStep(1),batchStep(2),batchStep(3));
}
void batchFault(const char* why){
    outputs.stop();batch.fault();routePrompt=0;batchOptical=false;
    const uint32_t now=millis();
    if(no2.active())no2.fault(now);
    if(no3.active())no3.fault(now);
    if(nh3.active())nh3.fault(now);
    no2Issued=no3Issued=nh3Issued=false;
    stopAll();Serial.printf("BATCH FAULT %s %s; no automatic retry or drain\n",batchId,why);batchStatus();
}
bool batchStart(uint32_t mask,bool scheduled){
    if(!WQ_ENABLE_BATCH_SUPERVISED||mask<1||mask>7||(scheduled&&mask!=7)||batch.held()
        ||assayBlocked(AssayId::None,no2,no3,nh3)||!batchIdleRemote()
        ||!WQ_ENABLE_OUTPUTS||!WQ_ENABLE_RS485||!dirtyStore.clean())return false;
    const uint8_t need=batchRequiredCal(uint8_t(mask));
    if((calMask&need)!=need)return false;
    uint32_t total=0;
    if(mask&1){if(!WQ_ENABLE_NO2_SUPERVISED||!no2Profile.valid())return false;total=no2Profile.totalDrainMs;}
    if(mask&2){if(!WQ_ENABLE_NO3_SUPERVISED||!no3Profile.valid()||(total&&total!=no3Profile.totalDrainMs))return false;total=no3Profile.totalDrainMs;}
    if(mask&4){if(!WQ_ENABLE_NH3_SUPERVISED||!nh3Profile.valid()||(total&&total!=nh3Profile.totalDrainMs))return false;total=nh3Profile.totalDrainMs;}
    if((mask&1)&&!no2Preflight())return false;
    if((mask&2)&&!no3Preflight())return false;
    if((mask&4)&&!nh3Preflight())return false;
    if(!dirtyStore.mark(uint8_t(mask))||!batch.start(uint8_t(mask),scheduled,millis()))return false;
    snprintf(batchId,sizeof(batchId),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    batchTotalMs=total;batchTotalIssued=false;routePrompt=0;batchOptical=false;
    Serial.printf("BATCH START %s mode=%s mask=%lu\n",batchId,scheduled?"scheduled":"manual",(unsigned long)mask);batchStatus();return true;
}
bool batchBeginPrep(uint8_t id){
    if(routePrompt!=id||elapsed(millis(),routeAt,120000)||!batchIdleRemote()||!batch.beginPrep(id,millis()))return false;
    routePrompt=0;bool ok=false;uint32_t now=millis();
    if(id==1){ok=no2.start(no2Profile,now);imageId=0;no2MoveLog();}
    else if(id==2){ok=no3.start(no3Profile,now);no3ImageId=0;no3MoveLog();}
    else if(id==3){ok=nh3.start(nh3Profile,now);nh3ImageId=0;nh3NewSession();nh3MoveLog();}
    if(!ok){batchFault("recipe_start");return false;}
    Serial.printf("BATCH SESSION {\"batch_id\":\"%s\",\"analyte\":\"%s\",\"session_id\":\"",batchId,batchName(id));
    if(id==3)Serial.print(nh3SessionId);else Serial.printf("%s-%u",batchId,unsigned(id));
    Serial.println("\"}");return true;
}
bool batchConfirm(uint8_t id){
    if(batch.work!=BatchWork::Prep||batch.owner!=id||!batchIdleRemote())return false;
    uint32_t now=millis();
    if(elapsed(now,batch.workAt,BATCH_PREP_MS)||batch.due(now)){batchFault("prep_deadline");return false;}
    bool ok=id==1?no2.confirm(now):id==2?no3.confirm(now):nh3.confirm(now);
    if(!ok)return false;
    if(id==1)no2MoveLog();else if(id==2)no3MoveLog();else nh3MoveLog();
    if(batchReaction(id)){
        if(!batch.endPrep(now)){batchFault("reaction_deadline_conflict");return false;}
        Serial.printf("BATCH REACTION %s %u start=%lu duration=%lu\n",batchId,unsigned(id),(unsigned long)now,(unsigned long)batchReactionMs(id));batchStatus();
    }
    return true;
}
bool batchConfirmImage(uint8_t id,uint32_t capture){
    if(batch.work!=BatchWork::Image||batch.owner!=id||!batchOptical||!batchImageIssued(id)
        ||capture!=batchCaptureId(id)||!batchIdleRemote()||elapsed(millis(),batch.workAt,BATCH_IMAGE_MS))return false;
    const uint32_t now=millis();
    bool ok=id==1?no2.imageConfirmed(now):id==2?no3.imageConfirmed(now):nh3.imageConfirmed(now);
    if(!ok)return false;
    if(id==1)no2MoveLog();else if(id==2)no3MoveLog();else nh3MoveLog();
    batchOptical=false;batch.endImage();batchStatus();return true;
}
void batchTick(){
    if(!batch.running())return;
    uint32_t now=millis();
    if(elapsed(now,batch.since,BATCH_LIMIT_MS)){batchFault("batch_timeout");return;}
    if(batch.work!=BatchWork::Image&&(!linked||elapsed(now,lastGood,LINK_LEASE_MS))){batchFault("link");return;}
    if(batch.work==BatchWork::Prep){
        if(elapsed(now,batch.workAt,BATCH_PREP_MS)||batch.due(now)){batchFault("prep_deadline");return;}
        const uint8_t id=batch.owner;batchRunTick(id);
        if(batchRecipeFault(id))batchFault("recipe_fault");
        return;
    }
    if(batch.work==BatchWork::Image){
        const uint8_t id=batch.owner;
        if(elapsed(now,batch.workAt,BATCH_IMAGE_MS)){batchFault("image_timeout");return;}
        for(uint8_t i=1;i<=3;++i)if(i!=id&&batch.remaining(i,now)==0){batchFault("camera_deadline_conflict");return;}
        if(!batchImageIssued(id)){
            if(!batch.dispatchAllowed(now)){batchFault("image_dispatch_late");return;}
            if(!batchIdleRemote()){batchFault("image_interlock");return;}
            if(!batchOptical)return;
        }
        batchRunTick(id);
        if(batchRecipeFault(id))batchFault("camera_fault");
        return;
    }
    if(batch.work==BatchWork::Drain){
        const uint8_t id=batch.owner;batchRunTick(id);
        if(batchRecipeFault(id)){batchFault("drain_fault");return;}
        if(batchTotalStep(id)){
            // The shared OUT7 must run once, after all selected vial drains.
            if(id==1)no2.move(No2Step::Complete,millis());
            else if(id==2)no3.move(No3Step::Complete,millis());
            else nh3.move(Nh3Step::Complete,millis());
            batch.endDrain();batchStatus();
        }
        return;
    }
    if(batch.work==BatchWork::Total){
        if(!linked||remoteState||remoteChannel){batchFault("total_interlock");return;}
        if(!batchTotalIssued){
            if(!batchIdleRemote()||!outputs.arm(now)||!outputs.pulse(7,uint16_t(batchTotalMs),8,now)){batchFault("total_start");return;}
            batchTotalAt=now;batchTotalIssued=true;
        }else if(!outputs.gate.channel){
            if(!outputs.gate.armed||!elapsed(now,batchTotalAt,batchTotalMs)){batchFault("total_interrupted");return;}
            outputs.stop();batch.finish();Serial.printf("BATCH COMPLETE %s; clean before reset\n",batchId);batchStatus();
        }
        return;
    }
    if(!batchIdleRemote()){batchFault("idle_interlock");return;}
    const uint8_t due=batch.due(now);
    if(due){
        routePrompt=0;batchOptical=false;
        if(!batch.beginImage(due,now)){batchFault("image_dispatch_late");return;}
        bool ok=due==1?no2.advanceReaction(now):due==2?no3.advanceReaction(now):nh3.advanceReaction(now);
        if(!ok){batchFault("reaction_state");return;}
        if(due==1)no2MoveLog();else if(due==2)no3MoveLog();else nh3MoveLog();
        Serial.printf("BATCH OPTICAL %s %u; position camera and confirm batch optical %u\n",batchId,unsigned(due),unsigned(due));return;
    }
    if(batch.imaged==batch.mask){
        const uint8_t drain=batch.nextDrain();
        if(drain){if(!batch.beginDrain(drain,now))batchFault("drain_state");}
        else if(!batch.beginTotal(now))batchFault("total_state");
        return;
    }
    const uint8_t next=batch.nextPrep();
    if(next&&batch.canPrep(next,now)){
        if(routePrompt!=next){routePrompt=next;routeAt=now;Serial.printf("BATCH ROUTE %s %u; isolate feed and confirm batch route %u\n",batchId,unsigned(next),unsigned(next));}
        else if(elapsed(now,routeAt,120000)){batchFault("route_timeout");}
    }else routePrompt=0;
}
bool batchCli(const char* cmd){
    uint32_t args[2]={};
    if(!strcmp(cmd,"batch status")){batchStatus();return true;}
    if(!strcmp(cmd,"batch start scheduled")){
        Serial.println(batchStart(7,true)?"BATCH ACCEPTED supervised scheduled request":"REJECT batch preflight");return true;
    }
    if(parseNumbers(cmd,"batch start manual",args,1)){
        Serial.println(batchStart(args[0],false)?"BATCH ACCEPTED supervised manual request":"REJECT batch preflight");return true;
    }
    if(!strcmp(cmd,"batch reset")){
        if(!batch.held()||batch.running()||!batchIdleRemote()){Serial.println("REJECT batch reset");return true;}
        no2.reset(millis());no3.reset(millis());nh3.reset(millis());
        no2Profile=No2Profile{};no3Profile=No3Profile{};nh3Profile=Nh3Profile{};
        nh3Evidence=SampleEvidence{};nh3SessionId[0]=0;imageId=no3ImageId=nh3ImageId=0;
        batch.reset();batchId[0]=0;routePrompt=0;Serial.println("BATCH RESET; operator confirms physical recovery and cleaning");return true;
    }
    if(!batch.held())return false;
    if(!strcmp(cmd,"stop")||!strcmp(cmd,"batch stop")){
        if(batch.running())batchFault("operator_stop");else stopAll();return true;
    }
    if(parseNumbers(cmd,"batch route",args,1)){
        Serial.println(args[0]>=1&&args[0]<=3&&batchBeginPrep(uint8_t(args[0]))?"BATCH ROUTE ACCEPTED":"REJECT route");return true;
    }
    if(parseNumbers(cmd,"batch confirm",args,1)){
        Serial.println(args[0]>=1&&args[0]<=3&&batchConfirm(uint8_t(args[0]))?"BATCH CONFIRMED":"REJECT confirmation");return true;
    }
    if(parseNumbers(cmd,"batch optical",args,1)){
        if(args[0]==batch.owner&&!batchOptical&&batchIdleRemote()&&batch.dispatchAllowed(millis())){
            batchOptical=true;Serial.println("BATCH OPTICAL ACCEPTED");
        }else Serial.println("REJECT optical");
        return true;
    }
    if(parseNumbers(cmd,"batch image",args,2)){
        Serial.println(args[0]>=1&&args[0]<=3&&batchConfirmImage(uint8_t(args[0]),args[1])?"BATCH IMAGE CONFIRMED":"REJECT image");return true;
    }
    if(!strcmp(cmd,"nh3 evidence")&&!outputs.gate.armed&&!remoteChannel){nh3EmitEvidence();return true;}
    Serial.println("REJECT batch owns controls; use batch status/route/confirm/optical/image/stop/reset");return true;
}


bool cleanIdleRemote(){return batchIdleRemote();}
bool assayRunning(){return batch.running()||no2.active()||no3.active()||nh3.active();}
void cleanStatus(){
    Serial.printf("CLEAN STATUS {\"step\":\"%s\",\"dirty_mask\":%u,\"selected_mask\":%u,\"verified_mask\":%u,\"vial\":%u,\"cycle\":%u,\"storage_ok\":%s,\"ready_for_assay\":%s}\n",
        cleanStepName(cleaning.step),unsigned(dirtyStore.mask()),unsigned(cleaning.mask),unsigned(cleaning.verified),unsigned(cleaning.owner),unsigned(cleaning.cycle),
        dirtyStore.healthy()?"true":"false",(dirtyStore.clean()&&!cleaning.active()&&cleaning.step!=CleanStep::Fault&&!assayRunning()&&!batch.held()&&!assayBlocked(AssayId::None,no2,no3,nh3)&&cleanIdleRemote())?"true":"false");
}
void cleanMoveLog(){
    cleanIssued=false;cleanStatus();
    if(cleaning.step==CleanStep::Route)Serial.printf("CLEAN ROUTE %u; confirm clean water, isolated feed, open waste path: clean route %u\n",unsigned(cleaning.owner),unsigned(cleaning.owner));
    if(cleaning.step==CleanStep::Verify)Serial.printf("CLEAN VERIFY %u; inspect empty vial and verify rinse procedure, then clean verify %u\n",unsigned(cleaning.owner),unsigned(cleaning.owner));
}
void cleanFault(const char* reason){
    outputs.stop();cleaning.fault(millis());cleanIssued=false;stopAll();
    Serial.printf("CLEAN FAULT %s; dirty mask retained; inspect manually before clean reset\n",reason);cleanStatus();
}
void cleanTick(){
    if(!cleaning.active())return;
    const uint32_t now=millis();
    if(cleaning.expired(now)){cleanFault("timeout");return;}
    if(!linked||elapsed(now,lastGood,LINK_LEASE_MS)||remoteState||remoteChannel||!dirtyStore.healthy()){cleanFault("interlock");return;}
    const uint8_t channel=cleaning.channel();
    if(channel){
        if(!cleanIssued){
            if(!cleanIdleRemote()||!outputs.arm(now)||!outputs.pulse(channel,uint16_t(cleaning.duration()),8,now)){
                cleanFault("pulse_rejected");return;
            }
            cleanPulseAt=now;cleanIssued=true;
        }else if(!outputs.gate.channel){
            if(!outputs.gate.armed||!elapsed(now,cleanPulseAt,cleaning.duration())){cleanFault("pulse_interrupted");return;}
            outputs.stop();
            if(!cleaning.pulseDone(now)){cleanFault("transition");return;}
            cleanMoveLog();
        }else if(outputs.gate.channel!=channel){cleanFault("channel_mismatch");}
        return;
    }
    if(!cleanIdleRemote()){cleanFault("idle_interlock");return;}
    if(cleaning.settleDone(now))cleanMoveLog();
}
bool cleanCli(const char* cmd){
    uint32_t v[7]={};
    if(!strcmp(cmd,"clean status")){cleanStatus();return true;}
    if(!strcmp(cmd,"clean reset")){
        if(assayRunning()||cleaning.active()||!cleanIdleRemote()){Serial.println("REJECT clean reset");return true;}
        cleaning.reset();for(auto& p:cleanProfiles)p=CleanProfile{};cleanIssued=false;
        Serial.println("CLEAN RESET; dirty mask unchanged");cleanStatus();return true;
    }
    if(parseNumbers(cmd,"clean profile",v,7)){
        if(assayRunning()||cleaning.step!=CleanStep::Idle||!cleanIdleRemote()||v[0]<1||v[0]>3){Serial.println("REJECT clean profile");return true;}
        CleanProfile p;p.sourceMs=v[1];p.fillMs=v[2];p.settleMs=v[3];p.drainMs=v[4];p.totalMs=v[5];p.cycles=v[6];
        cleanProfiles[v[0]-1]=p.valid()?p:CleanProfile{};Serial.println(p.valid()?"CLEAN PROFILE staged":"REJECT profile bounds");return true;
    }
    if(parseNumbers(cmd,"clean start",v,1)){
        if(!WQ_ENABLE_CLEAN_SUPERVISED||!WQ_ENABLE_OUTPUTS||!WQ_ENABLE_RS485||assayRunning()
            ||cleaning.step!=CleanStep::Idle||!cleanIdleRemote()||v[0]<1||v[0]>ALL_VIALS||!dirtyStore.healthy()){
            Serial.println("REJECT clean start");return true;
        }
        const uint8_t m=uint8_t(v[0]);
        for(uint8_t i=0;i<3;++i)if((m&(1u<<i))&&!cleanProfiles[i].valid()){Serial.println("REJECT missing clean profile");return true;}
        // Journal before any pulse, even if cleaning previously clean vials.
        if(!dirtyStore.mark(m)||!cleaning.start(m,cleanProfiles,millis())){Serial.println("REJECT dirty journal or plan");return true;}
        cleanMoveLog();return true;
    }
    if(parseNumbers(cmd,"clean route",v,1)){
        if(v[0]<1||v[0]>3||!cleanIdleRemote()||!cleaning.route(uint8_t(v[0]),millis()))Serial.println("REJECT clean route");
        else cleanMoveLog();
        return true;
    }
    if(parseNumbers(cmd,"clean verify",v,1)){
        if(v[0]<1||v[0]>3||!cleanIdleRemote()||!cleaning.verify(uint8_t(v[0]),millis())){Serial.println("REJECT clean verification");return true;}
        if(cleaning.step==CleanStep::Complete){
            if(cleaning.verified!=cleaning.mask||!dirtyStore.verified(cleaning.mask)){cleanFault("journal_commit");return true;}
            Serial.println("CLEAN COMPLETE operator-verified rinse, not chemical residue measurement");
        }
        cleanMoveLog();return true;
    }
    if((cleaning.active()||cleaning.step==CleanStep::Fault)&&(!strcmp(cmd,"stop")||!strcmp(cmd,"clean stop"))){if(cleaning.active())cleanFault("operator_stop");else stopAll();return true;}
    if(cleaning.active()||cleaning.step==CleanStep::Fault){Serial.println("REJECT cleaning owns controls or fault; use clean status/reset");return true;}
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
    Serial.printf(" cal_mask=0x%02x all_calibrated=%u dirty_mask=%u storage_ok=%u\n",unsigned(calMask),unsigned(calMask==0x7f),unsigned(dirtyStore.mask()),unsigned(dirtyStore.healthy()));
}
void setup(){
    outputs.begin();Serial.begin(115200);dirtyStore.begin();Wire.begin(9,8);Wire.setTimeOut(25);
    adcReady=ads.begin(WQ_ADS_ADDRESS,&Wire);if(adcReady)ads.setGain(GAIN_TWOTHIRDS);
    bus.begin(18,17);
    Serial.println("EITEK bench V2: status | inventory | arm | pulse CH MS | calpulse CH MS | volume CH UL | capture | stop");
    inventory(true,false);
    Serial.println("CLEAN supervised: clean status | clean profile ID SOURCE FILL SETTLE DRAIN TOTAL CYCLES | clean start MASK");
    cleanStatus();
    Serial.println("BATCH supervised: batch start manual MASK | batch start scheduled | batch status");
    Serial.println("NH3 supervised only: nh3 profile SOURCE_MS SAMPLE_MS R1_UL R2_UL DRAIN_MS TOTAL_MS | start | confirm | image ID | reset | status");
    Serial.println("NO3 supervised only: no3 profile SOURCE_MS SAMPLE_MS R1_UL R2_UL R3_UL DRAIN_MS TOTAL_MS | start | confirm | image ID | reset | status");
    Serial.println("NO2 supervised only: no2 profile/source/sample/r1/r2/drain/total (see docs), no2 start | confirm | image ID | reset | status");
}
void loop(){
    uint32_t now=millis();outputs.tick(now);
    if(elapsed(now,lastPoll,250)){lastPoll=now;bool ok=poll();if(!ok&&WQ_ENABLE_RS485)outputs.stop();}
    if(!cleaning.active()&&!batch.running()&&!no2.active()&&!no3.active()&&!nh3.active()&&!outputs.gate.channel&&remoteChannel==0&&elapsed(now,lastSample,1000)){lastSample=now;
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
    if(cleaning.active()||cleaning.step==CleanStep::Fault){
        const char* cmd=cli.read();if(cmd)cleanCli(cmd);
        cleanTick();return;
    }
    if(batch.held()){
        const char* cmd=cli.read();if(cmd&&!cleanCli(cmd))batchCli(cmd);
        batchTick();return;
    }
    no2Tick();no3Tick();nh3Tick();
    const char* s=cli.read();if(!s)return;
    if(cleanCli(s)||batchCli(s))return;
    unsigned ch=0,ms=0;uint32_t args[2]={};
    if(!strcmp(s,"stop")){if(no2.active())no2Fault("operator_stop");else if(no3.active())no3Fault("operator_stop");else if(nh3.active())nh3Fault("operator_stop");else stopAll();Serial.println("STOP");}
    else if(no2Cli(s)||no3Cli(s)||nh3Cli(s))return;
    else if(assayBlocked(AssayId::None,no2,no3,nh3)){Serial.println("REJECT assay owns controls; use no2/no3/nh3 status/reset");return;}
    else if(!strcmp(s,"status"))printStatus();
    else if(!strcmp(s,"inventory")){
        if(!outputs.gate.armed&&!remoteChannel&&remoteState!=STATE_CAMERA_BUSY)inventory(true,false);else Serial.println("REJECT busy");
    }else if(!strcmp(s,"capture")){
        if(!canMasterCapture(linked,outputs.gate.armed,remoteState,remoteChannel)){Serial.println("REJECT: disarm both controllers before capture");return;}
        if(command(CMD_CAPTURE,0)){remoteState=STATE_CAMERA_BUSY;Serial.printf("CAM REQUEST %u (JPEG on slave UART)\n",seq?unsigned(seq):65536u);}
        else Serial.println("REJECT: capture not acknowledged");
    }else if(!strcmp(s,"arm")){if(!WQ_ENABLE_RS485||!canMasterArm(linked,outputs.gate.channel!=0,remoteState,remoteChannel)){Serial.println("REJECT: link or remote busy/fault");return;}
        if(!dirtyStore.mark(ALL_VIALS)){Serial.println("REJECT dirty journal");return;}
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
