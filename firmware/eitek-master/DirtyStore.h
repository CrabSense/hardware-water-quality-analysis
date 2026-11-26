#pragma once
#include <Preferences.h>
#include "../shared/CleanPlan.h"
namespace wq {
class DirtyStore {
    Preferences prefs;
    uint8_t dirty=ALL_VIALS;
    bool ready=false,persisted=false;
    bool write(uint8_t m){
        if(!ready||m>ALL_VIALS)return false;
        const uint32_t value=encodeDirty(m);
        if(prefs.putUInt("dirty",value)!=sizeof(uint32_t)||prefs.getUInt("dirty",UINT32_MAX)!=value){
            ready=false;dirty=ALL_VIALS;persisted=false;return false;
        }
        dirty=m;persisted=true;return true;
    }
public:
    void begin(){
        ready=prefs.begin("wq-clean-v1",false);
        if(!ready)return;
        uint32_t v=prefs.getUInt("dirty",UINT32_MAX);
        dirty=decodeDirty(v);persisted=v==encodeDirty(dirty);
        // Persist conservative state if missing or malformed, before any output.
        if(!persisted)write(ALL_VIALS);
    }
    uint8_t mask()const{return ready?dirty:ALL_VIALS;}
    bool healthy()const{return ready&&persisted;}
    bool clean()const{return healthy()&&!dirty;}
    bool mark(uint8_t m){
        if(!healthy()||!validCleanMask(m))return false;
        uint8_t n=dirty|m;return n==dirty||write(n);
    }
    bool verified(uint8_t m){
        if(!healthy()||!validCleanMask(m)||(dirty&m)!=m)return false;
        return write(uint8_t(dirty&~m));
    }
};
}
