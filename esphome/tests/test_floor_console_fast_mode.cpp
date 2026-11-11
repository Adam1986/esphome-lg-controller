#include <cassert>
#include "floor_console_flow.h"
using namespace lg_floor_console;
int main() {
    // Snapshot taken immediately before an RX echo sets the timer: no 49-day
    // unsigned underflow and no premature UNVERIFIED / missing readback.
    assert(fast_elapsed_ms(1000u, 1003u) == 0u);
    assert(fast_elapsed_ms(1003u, 1003u) == 0u);
    assert(fast_elapsed_ms(1004u, 1003u) == 1u);
    assert(fast_elapsed_ms(3u, 0xfffffffdu) == 6u); // genuine rollover
    assert(!fast_verify_due(true, false, false, false,
                            fast_elapsed_ms(1000u, 1003u), 45000u));
    assert(!fast_verify_due(false, false, false, false, 50000, 45000));
    assert(!fast_verify_due(true, true, false, false, 50000, 45000));
    assert(!fast_verify_due(true, false, true, false, 50000, 45000));
    assert(!fast_verify_due(true, false, false, true, 50000, 45000));
    assert(!fast_verify_due(true, false, false, false, 44999, 45000));
    assert(fast_verify_due(true, false, false, false, 45000, 45000));
    // Custom 120s quiet time: a fresh fan/mode or direction request moves
    // the activity clock, but NEVER moves the absolute pending deadline.
    const uint32_t request_at = 10000u;
    uint32_t last_activity_at = request_at;
    assert(!fast_verify_due(true, false, false, false,
                            fast_elapsed_ms(120000u, last_activity_at), 120000u));
    last_activity_at = 100000u; // another explicit user control
    assert(!fast_verify_due(true, false, false, false,
                            fast_elapsed_ms(160000u, last_activity_at), 120000u));
    assert(fast_verify_due(true, false, false, false,
                           fast_elapsed_ms(220000u, last_activity_at), 120000u));
    // Absolute expiry remains bounded even when normal controls keep arriving.
    assert(fast_verify_expired(false, fast_elapsed_ms(265000u, request_at),
                               120000u, 135000u, 90000u));
    assert(!fast_verify_expired(false, 179999u, 45000u, 135000u, 90000u));
    assert(fast_verify_expired(false, 180000u, 45000u, 135000u, 90000u));
    assert(!fast_verify_expired(false, 254999u, 120000u, 135000u, 90000u));
    assert(fast_verify_expired(false, 255000u, 120000u, 135000u, 90000u));
    assert(!fast_verify_expired(true, 89999u, 120000u, 135000u, 90000u));
    assert(fast_verify_expired(true, 90000u, 120000u, 135000u, 90000u));
    // An old CD must never overwrite a new HA selection during queueing,
    // transmission or before the new guarded readback echo. Only subsequent
    // genuine indoor frames can resolve the current request.
    assert(suppress_unverified_discharge_cd(true, false, false, false));
    assert(suppress_unverified_discharge_cd(true, true, false, false));
    assert(suppress_unverified_discharge_cd(true, true, true, false));
    assert(!suppress_unverified_discharge_cd(true, true, true, true));
    assert(!suppress_unverified_discharge_cd(false, false, false, false));
    FastCDVerifier v;
    v.reset(2); // Bottom
    assert(v.observe(2) == FastCDResult::Waiting);
    assert(v.observe(2) == FastCDResult::Confirmed);
    v.reset(1); // Top
    assert(v.observe(2) == FastCDResult::Waiting);
    assert(v.observe(0) == FastCDResult::Waiting); // different mismatch resets
    assert(v.observe(0) == FastCDResult::Different);
    // V2.0.6: optional single-CD confirmation only for a *fresh matching*
    // indoor frame. Mismatch still requires two identical, consistent frames.
    v.reset(2);
    assert(v.observe(2, true) == FastCDResult::Confirmed);
    v.reset(1);
    assert(v.observe(2, true) == FastCDResult::Waiting);
    assert(v.observe(2, true) == FastCDResult::Different);
    v.reset(0);
    assert(v.observe(1, true) == FastCDResult::Waiting);
    assert(v.observe(0, true) == FastCDResult::Confirmed);
    v.reset(0); // Both
    assert(v.observe(1) == FastCDResult::Waiting);
    assert(v.observe(0) == FastCDResult::Waiting);
    assert(v.observe(0) == FastCDResult::Confirmed);
}
