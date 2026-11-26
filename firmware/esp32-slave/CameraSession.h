#pragma once
#include <Arduino.h>
#include <USB_STREAM.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include "../shared/Jpeg.h"
#include "../shared/Integration.h"

#ifndef WQ_CAMERA_WIDTH
#define WQ_CAMERA_WIDTH 320
#endif
#ifndef WQ_CAMERA_HEIGHT
#define WQ_CAMERA_HEIGHT 240
#endif
#ifndef WQ_CAMERA_FPS
#define WQ_CAMERA_FPS 15
#endif
namespace wq {
// Control state belongs only to loop(). The UVC callback owns no GPIO and
// communicates through a mutex-protected single snapshot.
class CameraSession {
    SemaphoreHandle_t mutex=nullptr;
    uint8_t *a=nullptr,*b=nullptr,*frameBuf=nullptr,*image=nullptr;
    bool host=false,acceptFrames=false,connected=false,ready=false;
    size_t size=0,pos=0; unsigned width=0,height=0;
    uint32_t sequence=0,checksum=0;
    bool headerSent=false,cancelled=false;
    static void stateCallback(usb_stream_state_t event,void* user){
        auto* self=static_cast<CameraSession*>(user);
        if(xSemaphoreTake(self->mutex,portMAX_DELAY)!=pdTRUE)return;
        self->connected=event==STREAM_CONNECTED;
        xSemaphoreGive(self->mutex);
    }
    static void frameCallback(uvc_frame_t* frame,void* user){
        if(!frame)return;
        auto* self=static_cast<CameraSession*>(user);
        if(xSemaphoreTake(self->mutex,0)!=pdTRUE)return;
        const auto* data=static_cast<const uint8_t*>(frame->data);
        if(self->acceptFrames&&self->connected&&!self->ready&&frame->frame_format==UVC_FRAME_FORMAT_MJPEG
            &&frame->width&&frame->height&&jpegEnvelope(data,frame->data_bytes)){
            memcpy(self->image,data,frame->data_bytes);
            self->size=frame->data_bytes;self->width=frame->width;self->height=frame->height;
            self->sequence=frame->sequence;self->ready=true;self->acceptFrames=false;
        }
        xSemaphoreGive(self->mutex);
    }
    void freeBuffers(){
        if(a)heap_caps_free(a);
        if(b)heap_caps_free(b);
        if(frameBuf)heap_caps_free(frameBuf);
        if(image)heap_caps_free(image);
        a=b=frameBuf=image=nullptr;
    }
    void error(const char* reason,bool safe){
        Serial.printf("CAM ERROR %lu %s\n",(unsigned long)gate.id,reason);
        if(safe)freeBuffers();
        gate.finish(safe);
    }
    bool stopHost(){
        if(!host)return true;
        xSemaphoreTake(mutex,portMAX_DELAY);acceptFrames=false;xSemaphoreGive(mutex);
        if(usb_streaming_stop()!=ESP_OK)return false;
        host=false;return true;
    }
    void start(){
        // Start/stop may block in the vendor library; caller guarantees all loads
        // are disarmed and ARM is rejected until stop returns successfully.
        if(!mutex)mutex=xSemaphoreCreateMutex();
        if(!mutex){error("memory",true);return;}
        const size_t cap=CAMERA_MAX_BYTES;
        a=static_cast<uint8_t*>(heap_caps_malloc(cap,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
        b=static_cast<uint8_t*>(heap_caps_malloc(cap,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
        frameBuf=static_cast<uint8_t*>(heap_caps_malloc(cap,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        image=static_cast<uint8_t*>(heap_caps_malloc(cap,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!a||!b||!frameBuf||!image){error("memory",true);return;}
        xSemaphoreTake(mutex,portMAX_DELAY);
        acceptFrames=true;connected=false;ready=false;size=0;
        xSemaphoreGive(mutex);
        uvc_config_t cfg{};
        cfg.frame_width=WQ_CAMERA_WIDTH;cfg.frame_height=WQ_CAMERA_HEIGHT;cfg.frame_interval=FPS2INTERVAL(WQ_CAMERA_FPS);
        cfg.xfer_buffer_size=cap;cfg.xfer_buffer_a=a;cfg.xfer_buffer_b=b;
        cfg.frame_buffer_size=cap;cfg.frame_buffer=frameBuf;
        cfg.format=UVC_FORMAT_MJPEG;cfg.frame_cb=frameCallback;cfg.frame_cb_arg=this;
        esp_err_t rc=uvc_streaming_config(&cfg);
        if(rc==ESP_OK)rc=usb_streaming_state_register(stateCallback,this);
        if(rc!=ESP_OK){error("configuration",false);return;}
        // On any ambiguous driver start failure retain buffers and latch Fault.
        if(usb_streaming_start()!=ESP_OK){error("start_failed_reboot_required",false);return;}
        host=true;gate.receiving(millis());
    }
    void exportStep(){
        char line[112];int n=0;
        if(!headerSent){
            n=snprintf(line,sizeof(line),"CAM BEGIN %lu %lu %u %u %u %08lx\n",
                (unsigned long)gate.id,(unsigned long)sequence,width,height,unsigned(size),(unsigned long)checksum);
        }else if(pos<size){
            static const char hex[]="0123456789abcdef";
            const size_t count=size-pos>32?32:size-pos;
            char payload[65];
            for(size_t j=0;j<count;++j){uint8_t v=image[pos+j];payload[2*j]=hex[v>>4];payload[2*j+1]=hex[v&15];}
            payload[count*2]=0;
            n=snprintf(line,sizeof(line),"CAM DATA %lu %u %s\n",(unsigned long)gate.id,unsigned(pos),payload);
        }else n=snprintf(line,sizeof(line),"CAM END %lu\n",(unsigned long)gate.id);
        if(n<=0||size_t(n)>=sizeof(line)){error("format",true);return;}
        if(Serial.availableForWrite()<n)return;
        if(Serial.write(reinterpret_cast<const uint8_t*>(line),size_t(n))!=size_t(n)){error("uart",true);return;}
        if(!headerSent)headerSent=true;
        else if(pos<size)pos+=(size-pos>32?32:size-pos);
        else {freeBuffers();gate.finish(true);}
    }
public:
    CameraGate gate;
    bool request(uint32_t id,bool armed){
        if(!gate.request(id,armed,millis()))return false;
        cancelled=false;headerSent=false;pos=0;return true;
    }
    void cancel(){if(gate.busy())cancelled=true;}
    void tick(){
        if(!gate.busy())return;
        if(cancelled){const bool safe=stopHost();error(safe?"cancelled":"stop_failed_reboot_required",safe);return;}
        if(gate.phase==CameraPhase::Queued){start();return;}
        if(gate.phase==CameraPhase::Receiving){
            bool got=false;
            if(xSemaphoreTake(mutex,0)==pdTRUE){got=ready;xSemaphoreGive(mutex);}
            if(!got&&!gate.timedOut(millis()))return;
            if(!stopHost()){error("stop_failed_reboot_required",false);return;}
            if(!got){error("timeout",true);return;}
            if(!jpegEnvelope(image,size)){error("invalid",true);return;}
            checksum=jpegCrc(image,size);gate.exporting(millis());return;
        }
        if(gate.phase==CameraPhase::Exporting){
            if(gate.timedOut(millis())){error("uart_timeout",true);return;}
            exportStep();
        }
    }
};
}
