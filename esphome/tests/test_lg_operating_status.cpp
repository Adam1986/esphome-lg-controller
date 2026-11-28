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

    // Unsupported wall units retain the baseline even with repeated zeros.
    FanFeedback fan;
    fan.set_heating_candidate(true);
    assert(!fan.observe(0, 1000));
    assert(!fan.observe(0, 2000));
    assert(!fan.stopped(2000));
    assert(fan.observe(45, 3000));
    assert(!fan.stopped(3000));
    assert(fan.observe(38, 4000));
    assert(fan.observe(0, 5000));
    assert(fan.stopped(5000));

    // Repeated C8 heating status must not erase fresh idle evidence.
    fan.set_heating_candidate(true);
    assert(fan.stopped(6000));
    assert(fan.observe(45, 7000));
    assert(!fan.stopped(7000));

    // Stale feedback falls back; later valid zero restores inference.
    assert(fan.observe(0, 8000));
    assert(fan.stopped(187999));
    assert(!fan.expire(187999));
    assert(fan.expire(188000));
    assert(!fan.stopped(188000));
    assert(!fan.expire(188001));
    assert(fan.observe(0, 190000));
    assert(fan.stopped(190000));

    // OFF/preheat/defrost/other mode always wins. Require new CF when
    // returning to heat even if an idle sample arrived during that state.
    fan.set_heating_candidate(false);
    assert(!fan.stopped(190001));
    assert(fan.observe(0, 191000));
    assert(!fan.stopped(191000));
    fan.set_heating_candidate(true);
    assert(!fan.stopped(192000));
    assert(fan.observe(0, 193000));
    assert(fan.stopped(193000));

    // A unit first seen running outside HEAT can still prove CF support.
    FanFeedback other;
    assert(other.observe(78, 10));
    other.set_heating_candidate(true);
    assert(!other.stopped(20));
    assert(other.observe(0, 30));
    assert(other.stopped(30));

    // Unsigned time difference survives millis() wrap.
    assert(fan.observe(0, UINT32_MAX - 999));
    assert(fan.stopped(1000));
    assert(!fan.expire(1000));
    assert(fan.expire(179000));
    assert(!fan.stopped(179000));
}
