#pragma once
#include <cstdint>

namespace esphome::lg_controller {

// V2: never acknowledge an Auto Dry / vane UI write on a Slave before a
// genuine indoor CA template has been observed and remains the latest Type-A
// template. Master operation is unchanged. Readback publication bypasses this.
class SlaveTypeAControlPreflight {
 public:
  static bool can_accept_user_write(bool slave, bool genuine_indoor_ca_seen,
                                    const uint8_t* latest_type_a_template) {
    if (!slave) return true;
    return genuine_indoor_ca_seen && latest_type_a_template != nullptr &&
           latest_type_a_template[0] == 0xCA;
  }
};

}  // namespace esphome::lg_controller
