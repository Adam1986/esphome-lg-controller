#include <cassert>
#include <cstdint>
#include <iostream>
#include "../components/lg_controller/lg_slave_sleep_sync.h"

int main() {
    using Guard = esphome::lg_controller::LgSlaveSleepSync::SleepSetGuard;
    // Never allow a sleep SET before reading both genuine status sources.
    assert(!Guard::can_set(false, 0x0A, false, 0x0A, true, false));
    assert(!Guard::can_set(true, 0x0A, false, 0x0A, true, false));
    assert(!Guard::can_set(false, 0x0A, true, 0x0A, true, false));
    // Real OFF (bit 1 clear) on either the Master or indoor unit forbids SET.
    assert(!Guard::can_set(true, 0x08, true, 0x0A, true, false));
    assert(!Guard::can_set(true, 0x0A, true, 0x08, true, false));
    assert(!Guard::can_set(true, 0x08, true, 0x08, true, true));
    // Optimistic ON in HA or a lingering OFF latch cannot bypass the guard.
    assert(!Guard::can_set(true, 0x0A, true, 0x0A, false, false));
    assert(!Guard::can_set(true, 0x0A, true, 0x0A, true, true));
    // Both genuinely ON, including HEAT or FAN_ONLY, allow sleep SET.
    assert(Guard::can_set(true, 0x12, true, 0x12, true, false));
    assert(Guard::can_set(true, 0x0A, true, 0x0A, true, false));
    std::cout << "Sleep set guard tests passed\n";
}
