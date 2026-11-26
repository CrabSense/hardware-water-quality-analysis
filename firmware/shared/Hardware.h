#pragma once
#include <Arduino.h>
#include "Bench.h"
// Defaults deliberately inhibit real output and RS485 transmission.
// Enable per target only after confirming board polarity and transceiver.
#ifndef WQ_ENABLE_OUTPUTS
#define WQ_ENABLE_OUTPUTS 0
#endif
#ifndef WQ_ACTIVE_LEVEL
#define WQ_ACTIVE_LEVEL HIGH
#endif
#ifndef WQ_ENABLE_RS485
#define WQ_ENABLE_RS485 0
#endif
#ifndef WQ_RS485_DIR
#define WQ_RS485_DIR -1
#endif
namespace wq {
class Outputs {
    const uint8_t* pins; uint8_t count; uint8_t current=0;
public:
    Gate gate;
    Outputs(const uint8_t* p,uint8_t n):pins(p),count(n){}
    void begin(){
        if(!WQ_ENABLE_OUTPUTS)return;
        for(uint8_t i=0;i<count;++i){digitalWrite(pins[i],!WQ_ACTIVE_LEVEL);pinMode(pins[i],OUTPUT);}
    }
    void apply(){
        if(!WQ_ENABLE_OUTPUTS)return;
        if(current==gate.channel)return;
        for(uint8_t i=0;i<count;++i)digitalWrite(pins[i],!WQ_ACTIVE_LEVEL);
        current=gate.channel;
        if(current)digitalWrite(pins[current-1],WQ_ACTIVE_LEVEL);
    }
    bool arm(uint32_t now){if(!WQ_ENABLE_OUTPUTS)return false;gate.arm(now);apply();return true;}
    bool pulse(uint8_t ch,uint16_t ms,uint8_t limit,uint32_t now){if(!WQ_ENABLE_OUTPUTS)return false;bool ok=gate.pulse(ch,ms,limit,now);apply();return ok;}
    void stop(){gate.disarm();apply();}
    void tick(uint32_t now){gate.tick(now);apply();}
};
class Bus {
    HardwareSerial& port;
    uint8_t buf[64];size_t len=0;uint32_t last=0;bool overflow=false;
public:
    explicit Bus(HardwareSerial& s):port(s){}
    void begin(int rx,int tx){
        if(!WQ_ENABLE_RS485)return;
        if(WQ_RS485_DIR>=0){digitalWrite(WQ_RS485_DIR,LOW);pinMode(WQ_RS485_DIR,OUTPUT);}
        port.begin(9600,SERIAL_8N1,rx,tx);
    }
    void send(const uint8_t* data,size_t n){
        if(!WQ_ENABLE_RS485)return;
        if(WQ_RS485_DIR>=0)digitalWrite(WQ_RS485_DIR,HIGH);
        port.write(data,n);port.flush();
        if(WQ_RS485_DIR>=0)digitalWrite(WQ_RS485_DIR,LOW);
    }
    size_t receive(uint8_t* out){
        if(!WQ_ENABLE_RS485)return 0;
        while(port.available()){
            int b=port.read();if(b<0)break;
            if(len<sizeof(buf))buf[len++]=uint8_t(b);else overflow=true;
            last=micros();
        }
        if((len||overflow)&&uint32_t(micros()-last)>=4100){
            size_t n=overflow?0:len;if(n)memcpy(out,buf,n);len=0;overflow=false;return n;
        }
        return 0;
    }
    void discard(){if(!WQ_ENABLE_RS485)return;while(port.available())port.read();len=0;overflow=false;}
};
class Line {
    char b[80];size_t n=0;bool bad=false;
public:
    const char* read(){
        while(Serial.available()){
            char c=char(Serial.read());
            if(c=='\r'||c=='\n'){if(bad){bad=false;n=0;continue;}if(!n)continue;b[n]=0;n=0;return b;}
            if(n<sizeof(b)-1)b[n++]=c;else bad=true;
        }
        return nullptr;
    }
};
}
