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
    FastCDVerifier v;
    v.reset(2); // Bottom
    assert(v.observe(2) == FastCDResult::Waiting);
    assert(v.observe(2) == FastCDResult::Confirmed);
    v.reset(1); // Top
    assert(v.observe(2) == FastCDResult::Waiting);
    assert(v.observe(0) == FastCDResult::Waiting); // different mismatch resets
    assert(v.observe(0) == FastCDResult::Different);
    v.reset(0); // Both
    assert(v.observe(1) == FastCDResult::Waiting);
    assert(v.observe(0) == FastCDResult::Waiting);
    assert(v.observe(0) == FastCDResult::Confirmed);
}
