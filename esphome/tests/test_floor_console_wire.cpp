#include <cassert>
#include <initializer_list>
#include <cstdint>
#include <cstring>
#include "../components/lg_controller/floor_console_wire.h"
using namespace lg_floor_console;

static void test(Page page, bool slave, uint8_t target, uint8_t src_value) {
    uint8_t source[13] = {0};
    const uint8_t header = uint8_t((slave ? 0xA8 : 0xC8) | uint8_t(page));
    for (unsigned i=1;i<12;i++) source[i] = uint8_t(0xA0 + i);
    source[0] = header;
    if (page == Page::Step) source[1]=0x10;
    switch(page) {
      case Page::Comfort: source[11] = uint8_t((source[11]&~0x04) | (src_value ? 0x04 : 0)); break;
      case Page::Discharge: source[8] = uint8_t((source[8] & ~0x03) | src_value); break;
      case Page::Step: source[5] = uint8_t((source[5]&0xF0) | src_value); break;
    }
    source[12]=checksum(source);
    uint8_t output[13]={};
    assert(build(output,source,page,slave,target));
    assert(output[0] == uint8_t((slave?0x28:0xA8)|uint8_t(page)));
    assert(value(output,page)==target);
    assert(checksum(output)==output[12]);
    if(page==Page::Comfort) {
        assert(output[7] == uint8_t(source[7] | 0x80));
        for (int i=1;i<12;i++) if(i!=7 && i!=11) assert(output[i]==source[i]);
        assert((output[11]^source[11])==0x04);
    } else if(page==Page::Discharge) {
        for (int i=1;i<12;i++) if(i!=8) assert(output[i]==source[i]);
        assert(((output[8]^source[8])&~0x03)==0);
    } else {
        for (int i=1;i<12;i++) if(i!=5) assert(output[i]==source[i]);
        assert((output[5]&0xF0)==(source[5]&0xF0));
        assert(output[1]==0x10);
    }
    // Reject inappropriate source roles, unsupported subpage, bad checksums
    uint8_t mutated[13]; std::memcpy(mutated,source,13);
    mutated[0] = uint8_t((slave?0xC8:0xA8)|uint8_t(page));
    mutated[12]=checksum(mutated);
    if (slave) {
        // Real indoor page may act as the Slave template when the physical
        // Master never emits AD or AE.10. Preserve all unrelated bytes.
        assert(build(output, mutated, page, slave, target));
        assert(output[0] == uint8_t(0x28 | uint8_t(page)));
        assert(value(output,page) == target);
        assert(output[12] == checksum(output));
        for (int i=1; i<12; ++i) {
            if (page == Page::Comfort && (i==7 || i==11)) continue;
            if (page == Page::Discharge && i==8) continue;
            if (page == Page::Step && i==5) continue;
            assert(output[i] == mutated[i]);
        }
    } else {
        assert(!build(output, mutated, page, slave, target));
    }
    // Never use another Slave as the source of a command, nor bad checksum.
    mutated[0] = uint8_t(0x28 | uint8_t(page));
    mutated[12] = checksum(mutated);
    assert(!build(output, mutated, page, slave, target));
    mutated[0]=source[0]; mutated[12]=source[12]^0xAA;
    assert(!build(output,mutated,page,slave,target));
    if(page==Page::Step) {
        std::memcpy(mutated,source,13); mutated[1]=0x11;
        mutated[12]=checksum(mutated);
        assert(!build(output,mutated,page,slave,target));
    }
    assert(!build(output,source,page,slave,7));
}
int main() {
  for (bool slave: {false,true}) {
    for (unsigned target=0;target<=1;target++) test(Page::Comfort,slave,target,1-target);
    for (unsigned target=0;target<=2;target++) test(Page::Discharge,slave,target,uint8_t((target+1)%3));
    for (unsigned target=0;target<=2;target++) test(Page::Step,slave,target,uint8_t((target+1)%3));
  }
  return 0;
}
