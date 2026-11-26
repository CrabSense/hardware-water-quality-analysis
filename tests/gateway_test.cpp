#include "../firmware/shared/GatewayProtocol.h"
#include <cassert>
using namespace wq;
int main(){
    GatewayRequest r;
    assert(r.parse("gw 0123456789abcdef 1 0123456789abcdef0123456789abcdef batch start scheduled"));
    assert(r.seq==1&&gwAllowed(r.cmd));
    assert(!r.parse("gw 0123456789abcdef 4294967296 0123456789abcdef0123456789abcdef batch reset"));
    assert(!r.parse("gw 0123456789abcdef 0 0123456789abcdef0123456789abcdef batch reset"));
    assert(!r.parse("gw 0123456789abcdef +1 0123456789abcdef0123456789abcdef batch reset"));
    assert(!r.parse("gw 0123456789abcdef 1 0123456789abcdef0123456789abcdeg batch reset"));
    assert(!gwAllowed("pulse 1 100"));assert(!gwAllowed("arm"));assert(!gwAllowed("no2 start"));
    assert(gwAllowed("clean profile 1 100 100 0 100 100 1"));
    assert(!gwAllowed("batch confirm 1 extra"));
    GatewaySequence s;assert(!s.consume(2));assert(s.consume(1));assert(!s.consume(1));assert(!s.consume(3));assert(s.consume(2));
    s.last=UINT32_MAX;assert(!s.consume(0));assert(!s.consume(1));
}
