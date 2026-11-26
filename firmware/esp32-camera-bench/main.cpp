#include <Arduino.h>
#include <USB_STREAM.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include "../shared/Jpeg.h"
#include "../shared/Hardware.h"

// Standalone diagnostic only. No Outputs, Modbus, pumps or network.
// Use USB-UART connector for the console; native USB is dedicated to the camera.
static SemaphoreHandle_t lock=nullptr;
static uint8_t *a=nullptr,*b=nullptr,*frameBuffer=nullptr,*snapshot=nullptr;
static bool started=false,connected=false,pending=false,ready=false;
static uint32_t requestedAt=0,requestId=0,frameSeq=0,capturedAt=0;
static uint32_t accepted=0,rejected=0;
static unsigned width=0,height=0;
static size_t bytes=0;
static wq::Line cli;
static constexpr unsigned CAP=wq::CAMERA_MAX_BYTES;
#ifndef WQ_CAMERA_WIDTH
#define WQ_CAMERA_WIDTH 320
#endif
#ifndef WQ_CAMERA_HEIGHT
#define WQ_CAMERA_HEIGHT 240
#endif
#ifndef WQ_CAMERA_FPS
#define WQ_CAMERA_FPS 15
#endif

static void stateChanged(usb_stream_state_t event,void*){
    if(xSemaphoreTake(lock,portMAX_DELAY)!=pdTRUE)return;
    connected=(event==STREAM_CONNECTED);
    if(!connected){pending=false;ready=false;bytes=0;}
    xSemaphoreGive(lock);
}
static void onFrame(uvc_frame_t* frame,void*){
    if(!frame)return;
    const auto* data=static_cast<const uint8_t*>(frame->data);
    if(xSemaphoreTake(lock,0)!=pdTRUE)return;
    if(frame->frame_format!=UVC_FRAME_FORMAT_MJPEG||!wq::jpegEnvelope(data,frame->data_bytes)
        ||!frame->width||!frame->height){++rejected;xSemaphoreGive(lock);return;}
    ++accepted;
    if(connected&&pending&&!wq::elapsed(millis(),requestedAt,wq::CAMERA_TIMEOUT_MS)){
        memcpy(snapshot,data,frame->data_bytes);
        bytes=frame->data_bytes;width=frame->width;height=frame->height;
        frameSeq=frame->sequence;capturedAt=millis();pending=false;ready=true;
    }
    xSemaphoreGive(lock);
}
static void status(){
    if(!lock){Serial.println("CAM STATUS init_error");return;}
    xSemaphoreTake(lock,portMAX_DELAY);
    const bool c=connected,p=pending,r=ready;
    const uint32_t good=accepted,bad=rejected;
    xSemaphoreGive(lock);
    Serial.printf("CAM STATUS started=%u connected=%u pending=%u ready=%u valid=%lu rejected=%lu\n",
        unsigned(started),unsigned(c),unsigned(p),unsigned(r),(unsigned long)good,(unsigned long)bad);
}
static bool capture(uint32_t id){
    if(!started)return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool ok=connected&&!pending&&!ready;
    if(ok){requestId=id;requestedAt=millis();pending=true;bytes=0;}
    xSemaphoreGive(lock);
    return ok;
}
static void emit(){
    if(!started)return;
    // Only loop() issues requests. Callback may write snapshot only with pending=true.
    // Reserve completed snapshot, release mutex before slow UART output.
    xSemaphoreTake(lock,portMAX_DELAY);
    if(pending&&wq::elapsed(millis(),requestedAt,wq::CAMERA_TIMEOUT_MS)){
        uint32_t id=requestId;pending=false;xSemaphoreGive(lock);
        Serial.printf("CAM ERROR %lu timeout\n",(unsigned long)id);return;
    }
    if(!ready){xSemaphoreGive(lock);return;}
    const size_t n=bytes;const unsigned w=width,h=height;
    const uint32_t id=requestId,seq=frameSeq,age=millis()-capturedAt;
    ready=false;xSemaphoreGive(lock);
    if(!wq::jpegEnvelope(snapshot,n)||age>=wq::CAMERA_TIMEOUT_MS){
        Serial.printf("CAM ERROR %lu stale_or_invalid\n",(unsigned long)id);return;
    }
    Serial.printf("CAM BEGIN %lu %lu %u %u %u %08lx\n",(unsigned long)id,(unsigned long)seq,w,h,
        unsigned(n),(unsigned long)wq::jpegCrc(snapshot,n));
    static const char hex[]="0123456789abcdef";
    for(size_t pos=0;pos<n;){
        size_t count=n-pos>96?96:n-pos;char line[193];
        for(size_t j=0;j<count;++j){uint8_t v=snapshot[pos+j];line[j*2]=hex[v>>4];line[j*2+1]=hex[v&15];}
        line[count*2]=0;
        Serial.printf("CAM DATA %lu %u %s\n",(unsigned long)id,unsigned(pos),line);
        pos+=count;delay(1);
    }
    Serial.printf("CAM END %lu\n",(unsigned long)id);
}
static void initialize(){
    lock=xSemaphoreCreateMutex();
    // Driver transfer buffers in internal RAM; independent snapshot in PSRAM.
    a=static_cast<uint8_t*>(heap_caps_malloc(CAP,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    b=static_cast<uint8_t*>(heap_caps_malloc(CAP,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    frameBuffer=static_cast<uint8_t*>(heap_caps_malloc(CAP,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    snapshot=static_cast<uint8_t*>(heap_caps_malloc(CAP,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!lock||!a||!b||!frameBuffer||!snapshot){Serial.println("CAM ERROR 0 memory");return;}
    uvc_config_t cfg{};
    cfg.frame_width=WQ_CAMERA_WIDTH;cfg.frame_height=WQ_CAMERA_HEIGHT;cfg.frame_interval=FPS2INTERVAL(WQ_CAMERA_FPS);
    cfg.xfer_buffer_size=CAP;cfg.xfer_buffer_a=a;cfg.xfer_buffer_b=b;
    cfg.frame_buffer_size=CAP;cfg.frame_buffer=frameBuffer;
    cfg.format=UVC_FORMAT_MJPEG;cfg.frame_cb=onFrame;
    esp_err_t err=uvc_streaming_config(&cfg);
    if(err==ESP_OK)err=usb_streaming_state_register(stateChanged,nullptr);
    if(err==ESP_OK)err=usb_streaming_start();
    started=err==ESP_OK;
    Serial.printf("CAM INIT %s code=%d\n",started?"ok":"error",int(err));
}
void setup(){
    Serial.begin(115200);Serial.println("CrabSense CAMERA BENCH - physically disconnect load power");
    initialize();
}
void loop(){
    emit();
    const char* s=cli.read();
    if(s){
        if(!strcmp(s,"status"))status();
        else {
            unsigned long id=0;char extra;
            if(sscanf(s,"capture %lu %c",&id,&extra)==1&&id&&id<=UINT32_MAX&&strncmp(s,"capture ",8)==0){
                if(capture(uint32_t(id)))Serial.printf("CAM WAIT %lu\n",id);
                else Serial.printf("CAM ERROR %lu not_ready_or_busy\n",id);
            }else Serial.println("CAM commands: status | capture ID");
        }
    }
    delay(2);
}
