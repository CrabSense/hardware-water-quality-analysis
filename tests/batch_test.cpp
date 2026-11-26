#include "../firmware/shared/BatchPlan.h"
#include <cassert>
using namespace wq;
int main(){
    BatchPlan b;
    assert(!b.start(0,false,0));assert(!b.start(8,false,0));assert(!b.start(3,true,0));
    assert(b.start(7,true,0));assert(!b.start(7,true,0));assert(batchRequiredCal(7)==0x7f);
    assert(batchRequiredCal(1)==3&&batchRequiredCal(2)==0x1c&&batchRequiredCal(4)==0x60);
    assert(b.nextPrep()==3);assert(!b.beginPrep(1,0));assert(b.beginPrep(3,0));
    assert(!b.beginPrep(1,1));assert(b.endPrep(20000));assert(b.reacting==4);
    assert(b.beginPrep(1,21000));assert(b.endPrep(41000));assert(b.reacting==5);
    assert(b.beginPrep(2,42000));assert(b.endPrep(62000));assert(b.reacting==7);
    assert(!b.beginDrain(1,62000));assert(!b.beginTotal(62000));
    assert(b.due(181999)==0);assert(b.due(182000)==2);assert(b.beginImage(2,182000));
    assert(b.dispatchAllowed(191999));assert(!b.dispatchAllowed(192000));
    assert(!b.beginImage(1,182001));assert(b.endImage());assert(b.imaged==2);assert(!b.nextDrain());
    assert(b.beginImage(1,341000));assert(b.endImage());
    assert(b.beginImage(3,620000));assert(b.endImage());assert(b.imaged==7);
    for(uint8_t id=1;id<=3;++id){assert(b.nextDrain()==id);assert(b.beginDrain(id,621000));assert(b.endDrain());}
    assert(b.beginTotal(622000));assert(b.finish());assert(b.held()&&!b.running());assert(!b.start(1,false,0));assert(b.reset());
    assert(b.start(2,false,0));assert(b.nextPrep()==2);assert(b.beginPrep(2,0));assert(!b.endPrep(BATCH_PREP_MS));b.fault();assert(b.reset());
    assert(b.start(3,false,0));assert(b.beginPrep(1,0));assert(b.endPrep(1000));
    assert(b.canPrep(2,1001));assert(!b.canPrep(2,201000));
    assert(b.beginPrep(2,1001));assert(b.endPrep(2000));b.fault();assert(b.reset());
    assert(b.start(3,false,0));assert(b.beginPrep(1,0));assert(b.endPrep(0));
    b.reactionAt[0]=uint32_t(0)-180000u;
    assert(b.beginPrep(2,0));assert(!b.endPrep(0));b.fault();assert(b.reset());
    assert(b.start(1,false,UINT32_MAX-100));assert(b.beginPrep(1,UINT32_MAX-100));assert(b.endPrep(UINT32_MAX-100));
    assert(!b.due(299898));assert(b.due(299899)==1);assert(!b.beginImage(1,309899));assert(b.beginImage(1,299899));assert(b.endImage());
    assert(!b.beginTotal(300000));assert(b.beginDrain(1,300000));assert(b.endDrain());assert(b.beginTotal(300001));assert(b.finish());
}
