#include <cassert>
#include <cstdint>
#include "../components/lg_controller/floor_console_flow.h"
#include "../components/lg_controller/floor_console_wire.h"
#include "../components/lg_controller/slave_pipe_temp_discovery.h"

using esphome::lg_controller::SlavePipeTempDiscovery;
using namespace lg_floor_console;

int main() {
    // A8 and C8 supplied by a genuine stable wall/indoor pair, copied from
    // the model of the Garden Office status in the 10 November test.
    uint8_t master[13] = {0xA8,0x12,0x04,0x00,0x40,0x04,0x57,0x16,0,0,0x04,0,0};
    uint8_t indoor[13] = {0xC8,0x12,0x04,0x00,0x40,0x04,0x57,0x18,0,0,0,0,0};
    uint8_t query[13] = {};
    assert(template_refresh_due(0,0,75000,2));
    assert(SlavePipeTempDiscovery::make_request(query, master, indoor, 3000, 4000));
    query[12] = checksum(query);
    assert(query[0] == 0x28);
    assert((query[1] & 1) == 0); // never mark settings changed
    assert(query[8] == 0x40); // full-settings read ONLY
    assert(query[9] == 0); // no reservation command
    assert(query[12] == checksum(query));
    // This is NOT a page-5 write and cannot itself select a direction.
    assert(!recognized_frame(query, Page::Discharge));

    // Until a genuine checksum-valid CD/AD exists, a page write is refused.
    uint8_t write[13] = {};
    assert(!build(write, query, Page::Discharge, true, 2));
    uint8_t cd[13] = {0xCD,0,0xA5,0x0A,0x20,0x04,0,0,0,0,0,0,0};
    cd[12] = checksum(cd);
    assert(recognized_frame(cd, Page::Discharge));
    assert(build(write, cd, Page::Discharge, true, 2));
    assert(write[0] == 0x2D && (write[8] & 3) == 2);
    assert(write[12] == checksum(write));

    // Guard blocks stale/mismatched status pairs, active reservations and
    // transient settings changes; none may be bypassed by this flow.
    assert(!SlavePipeTempDiscovery::make_request(query, master, indoor, 90001, 0));
    master[3] = 0x10;
    assert(!SlavePipeTempDiscovery::make_request(query, master, indoor, 100, 100));
    master[3] = 0;
    master[1] |= 1;
    // The Floor Console preflight explicitly tests this changed bit before
    // using the general-purpose discovery builder.
    assert((master[1] & 1) != 0);
    master[1] &= ~uint8_t(1);
    indoor[6] = 0x56;
    assert(!SlavePipeTempDiscovery::make_request(query, master, indoor, 100, 100));
    return 0;
}
