#include <cassert>
#include <cstdint>
#include <cstring>
#include "floor_console_wire.h"

int main() {
    // Captured Garden Office 2D discharge-to-Bottom command.
    const uint8_t sent_bottom[13] = {
        0x2D,0x00,0xA5,0x0A,0x20,0x04,0x00,0x00,0x02,0x00,0x00,0x00,0x57
    };
    uint8_t probe[13] = {};
    assert(lg_floor_console::build_selective_cd_probe(probe, sent_bottom));
    const uint8_t expected[13] = {
        0x2D,0x80,0xA5,0x0A,0x20,0x04,0x00,0x00,0x02,0x00,0x00,0x00,0xD7
    };
    assert(std::memcmp(probe, expected, sizeof(probe)) == 0);
    assert(lg_floor_console::checksum(probe) == probe[12]);
    // If the candidate is accidentally interpreted as a write, it preserves
    // the last requested direction and every other settings byte except the
    // deliberately experimental byte1 0x80 bit.
    for (int i=0; i<12; ++i) {
        if (i != 1) assert(probe[i] == sent_bottom[i]);
    }
    assert((probe[8] & 0x03u) == 2);
    uint8_t bad[13]; std::memcpy(bad, sent_bottom, 13); bad[12] ^= 1;
    assert(!lg_floor_console::build_selective_cd_probe(probe, bad));
    std::memcpy(bad, sent_bottom, 13); bad[0] = 0xCD; bad[12] = lg_floor_console::checksum(bad);
    assert(!lg_floor_console::build_selective_cd_probe(probe, bad));
    assert(!lg_floor_console::build_selective_cd_probe(probe, nullptr));
    return 0;
}
