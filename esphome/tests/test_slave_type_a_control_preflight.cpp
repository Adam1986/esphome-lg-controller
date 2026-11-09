#include <cassert>
#include <cstdint>
#include "../components/lg_controller/slave_type_a_control_preflight.h"

using esphome::lg_controller::SlaveTypeAControlPreflight;

int main() {
    uint8_t missing[13] = {};
    uint8_t master_aa[13] = {0xAA};
    uint8_t indoor_ca[13] = {0xCA};
    // Never block a genuine Master simply because a CA hasn't been observed.
    assert(SlaveTypeAControlPreflight::can_accept_user_write(false, false, missing));
    assert(SlaveTypeAControlPreflight::can_accept_user_write(false, false, nullptr));
    // Fail closed on Slave: must be a genuine indoor CA, not a Master AA or stale guess.
    assert(!SlaveTypeAControlPreflight::can_accept_user_write(true, false, missing));
    assert(!SlaveTypeAControlPreflight::can_accept_user_write(true, false, indoor_ca));
    assert(!SlaveTypeAControlPreflight::can_accept_user_write(true, true, nullptr));
    assert(!SlaveTypeAControlPreflight::can_accept_user_write(true, true, missing));
    assert(!SlaveTypeAControlPreflight::can_accept_user_write(true, true, master_aa));
    assert(SlaveTypeAControlPreflight::can_accept_user_write(true, true, indoor_ca));
}
