#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "../shared/DosingCalibration.h"
namespace wq {
class CalibrationStore {
    Preferences prefs;
    bool ready=false;
    Calibration active[7],draft[7];
    static void key(uint8_t ch,char (&name)[8]){snprintf(name,sizeof(name),"pump%u",unsigned(ch));}
public:
    void begin(){
        ready=prefs.begin("wq-dose-v1",false);
        if(!ready)return;
        for(uint8_t ch=1;ch<=7;++ch){
            char name[8];key(ch,name);uint8_t b[CAL_BYTES];
            if(prefs.getBytesLength(name)==sizeof(b)&&prefs.getBytes(name,b,sizeof(b))==sizeof(b))
                decodeCalibration(b,ch,active[ch-1]);
        }
    }
    uint16_t mask()const{
        uint16_t m=0;
        for(uint8_t i=0;i<7;++i)if(active[i].valid())m|=uint16_t(1u<<i);
        return m;
    }
    bool duration(uint8_t ch,uint32_t ul,uint16_t& ms)const{
        ms=0;return ch>=1&&ch<=7&&active[ch-1].duration(ul,ms);
    }
    bool clear(uint8_t ch){
        if(ch<1||ch>7)return false;
        active[ch-1]=Calibration{};draft[ch-1]=Calibration{};
        if(!ready)return false;
        char name[8];key(ch,name);
        return !prefs.isKey(name)||prefs.remove(name);
    }
    bool stage(uint8_t ch,uint32_t low,const uint32_t (&a)[3],uint32_t high,const uint32_t (&b)[3]){
        if(ch<1||ch>7)return false;
        // Invalidate previously active data before any new calibration attempt.
        if(!clear(ch))return false;
        return fit(low,a,high,b,draft[ch-1]);
    }
    bool verify(uint8_t ch,uint32_t ms,uint32_t ul){
        if(!ready||ch<1||ch>7)return false;
        Calibration c=draft[ch-1];c.checkMs=ms;c.checkUl=ul;
        if(!c.valid())return false;
        uint8_t data[CAL_BYTES],readback[CAL_BYTES];encodeCalibration(c,ch,data);
        char name[8];key(ch,name);
        if(prefs.putBytes(name,data,sizeof(data))!=sizeof(data)||
           prefs.getBytes(name,readback,sizeof(readback))!=sizeof(readback)||
           memcmp(data,readback,sizeof(data))||!decodeCalibration(readback,ch,active[ch-1])){
            active[ch-1]=Calibration{};prefs.remove(name);return false;
        }
        draft[ch-1]=Calibration{};return true;
    }
    void print()const{
        Serial.printf("CAL mask=0x%02x storage=%s\n",unsigned(mask()),ready?"ready":"error");
        for(unsigned i=0;i<7;++i){
            const auto& c=active[i];
            Serial.printf("CAL CH%u %s lo_ms=%lu lo_ul=%lu hi_ms=%lu hi_ul=%lu check_ms=%lu check_ul=%lu draft=%u\n",
                i+1,c.valid()?"verified":"unverified",(unsigned long)c.loMs,(unsigned long)c.loUl,
                (unsigned long)c.hiMs,(unsigned long)c.hiUl,(unsigned long)c.checkMs,(unsigned long)c.checkUl,unsigned(draft[i].fitted()));
        }
    }
    bool handle(const char* s){
        if(!strcmp(s,"cal list")){print();return true;}
        uint32_t v[9]={};
        if(parseNumbers(s,"cal clear",v,1)){
            Serial.println(v[0]>=1&&v[0]<=7&&clear(uint8_t(v[0]))?"CAL CLEARED":"CAL ERROR clear_failed");return true;
        }
        if(parseNumbers(s,"cal fit",v,9)){
            uint32_t a[3]={v[2],v[3],v[4]},b[3]={v[6],v[7],v[8]};
            Serial.println(v[0]>=1&&v[0]<=7&&stage(uint8_t(v[0]),v[1],a,v[5],b)?"CAL FITTED verify_required":"CAL ERROR invalid_fit_or_storage");return true;
        }
        if(parseNumbers(s,"cal verify",v,3)){
            Serial.println(v[0]>=1&&v[0]<=7&&verify(uint8_t(v[0]),v[1],v[2])?"CAL VERIFIED":"CAL ERROR invalid_check_or_storage");return true;
        }
        return false;
    }
};
}
