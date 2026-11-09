#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::lg_controller {

// Accept a unit's C9 capabilities only after two identical (checksum-verified
// by the caller) frames. This avoids a single transient frame changing the
// Home Assistant climate feature list or causing unnecessary NVS writes.
class LgCapabilityRefresh {
 public:
  static constexpr std::size_t MESSAGE_LENGTH = 13;

  enum class Result : uint8_t {
    UNCHANGED,
    WAITING_FOR_REPEAT,
    ACCEPTED_WITHOUT_TRAIT_CHANGE,
    ACCEPTED_WITH_TRAIT_CHANGE,
  };

  Result observe(const uint8_t *incoming, uint8_t *stored) {
    // Byte 12 is the checksum. Compare payload bytes only.
    if (std::memcmp(stored, incoming, MESSAGE_LENGTH - 1) == 0) {
      candidate_valid_ = false;
      return Result::UNCHANGED;
    }
    if (!candidate_valid_ ||
        std::memcmp(candidate_, incoming, MESSAGE_LENGTH - 1) != 0) {
      std::memcpy(candidate_, incoming, MESSAGE_LENGTH);
      candidate_valid_ = true;
      return Result::WAITING_FOR_REPEAT;
    }

    candidate_valid_ = false;
    const bool traits_changed = changes_climate_traits(stored, incoming);
    std::memcpy(stored, incoming, MESSAGE_LENGTH);
    return traits_changed ? Result::ACCEPTED_WITH_TRAIT_CHANGE
                          : Result::ACCEPTED_WITHOUT_TRAIT_CHANGE;
  }

  static bool changes_climate_traits(const uint8_t *old_msg, const uint8_t *new_msg) {
    if (old_msg[0] != 0xC9) return true;
    // These are the only C9 fields used by configure_capabilities():
    //   byte 1: swing (horizontal/vertical)
    //   byte 2: heat/fan/auto/dry modes
    //   byte 3: auto/slow/low/medium/power fan modes
    // Mask the values that actually affect the advertised HA climate traits.
    return ((old_msg[1] ^ new_msg[1]) & 0xC0) != 0 ||
           ((old_msg[2] ^ new_msg[2]) & 0xC8) != 0 ||
           ((old_msg[3] ^ new_msg[3]) & 0x3B) != 0;
  }

 private:
  uint8_t candidate_[MESSAGE_LENGTH]{};
  bool candidate_valid_{false};
};

}  // namespace esphome::lg_controller
