#include <cassert>
#include <cstdio>
#include "../components/lg_controller/lg_filter_time.h"
using esphome::lg_controller::LgFilterTimeRemaining;
int main() {
    LgFilterTimeRemaining f;
    uint8_t cc[13] = {0xCC, 0xA2, 0x01, 0,0,0,0,0,0,0,0,0,0}; // ThinQ reference 418 hours
    assert(LgFilterTimeRemaining::decode(cc) == 418);
    assert(!f.confirmed());
    assert(!f.observe_indoor_cc(cc));
    assert(f.observe_indoor_cc(cc));
    assert(f.confirmed() && f.hours() == 418);
    assert(!f.observe_indoor_cc(cc));
    cc[0]=0xAC; cc[1]=0x33; cc[2]=0x49; // Master AC is not unit evidence
    assert(!f.observe_indoor_cc(cc));
    cc[0]=0xCC;
    assert(LgFilterTimeRemaining::decode(cc)==2355);
    assert(!f.observe_indoor_cc(cc));
    assert(f.hours()==418); // do not publish one-off changed CC
    assert(f.observe_indoor_cc(cc) && f.hours()==2355);
    cc[2]=0xC9; // occupancy/dual-setpoint bits must be ignored
    assert(LgFilterTimeRemaining::decode(cc)==2355);
    assert(!f.observe_indoor_cc(cc));
    cc[1]=0; cc[2]=0;
    assert(!f.observe_indoor_cc(cc));
    assert(f.observe_indoor_cc(cc) && f.hours()==0); // zero is valid
    puts("PASS: real indoor CC only, 12-bit filter remaining, dual-setpoint masking, two consistent samples");
}
