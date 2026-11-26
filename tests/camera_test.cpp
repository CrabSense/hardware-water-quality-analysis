#include "../firmware/shared/Jpeg.h"
#include <cassert>
int main(){
    const uint8_t jpeg[]={0xff,0xd8,0x00,0xff,0xd9};
    assert(wq::jpegEnvelope(jpeg,sizeof(jpeg)));
    assert(!wq::jpegEnvelope(jpeg,3));assert(!wq::jpegEnvelope(nullptr,5));
    assert(!wq::jpegEnvelope(jpeg,wq::CAMERA_MAX_BYTES+1));
    const uint8_t digits[]={'1','2','3','4','5','6','7','8','9'};
    assert(wq::jpegCrc(digits,9)==0xcbf43926u);
}
