#include <cassert>
#include <cstdint>
#include "lg_operating_status.h"

using namespace esphome::lg_controller;

int main() {
    // Slave must never let a wall-controller A8 overwrite C8 telemetry.
    assert(accepts_operating_status(true, 0xC8));
    assert(!accepts_operating_status(true, 0xA8));
    assert(!accepts_operating_status(true, 0x28));
    assert(accepts_operating_status(false, 0xC8));
    assert(accepts_operating_status(false, 0x28));  // Master path unchanged.

    OutdoorStatus outdoor;
    assert(outdoor.observe(false, 0));
    assert(outdoor.known && !outdoor.on);
    assert(outdoor.observe(true, 1000));
    assert(!outdoor.observe(false, 2000));
    assert(!outdoor.observe(false, 3000));
    assert(!outdoor.observe(true, 4000));
    assert(!outdoor.observe(false, 8999));
    assert(outdoor.changed_at == 1000);
    assert(outdoor.observe(false, 9000));
    assert(!outdoor.on);
    assert(!outdoor.observe(false, 10000));
    assert(outdoor.observe(true, 12000));
    assert(outdoor.observe(false, 21000));

    // The millis() counter may wrap while a short startup debounce is active.
    OutdoorStatus wrap;
    assert(wrap.observe(true, UINT32_MAX - 3000));
    assert(!wrap.observe(false, 1000));
    assert(wrap.observe(false, 5000));

    // Unsupported wall units remain unknown even if all CF frames are zero.
    FanFeedback fan;
    assert(!fan.observe(0));
    assert(!fan.observe(0));
    assert(fan.observe(45));
    assert(fan.observe(38));
    assert(fan.observe(0));  // Now zero is a meaningful observed value.
}
