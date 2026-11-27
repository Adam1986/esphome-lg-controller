#pragma once

#include <cstdint>

namespace esphome::lg_controller {

// Slave operating feedback must originate from the indoor unit. Master
// settings/temperature and command acceptance have their own receive paths.
inline bool accepts_operating_status(bool slave, uint8_t address) {
    return !slave || address == 0xC8;
}

// Suppress the brief ON -> OFF -> ON chatter at outdoor startup. The
// timestamp tracks a *published transition*, not a rejected raw reading.
struct OutdoorStatus {
    bool known = false;
    bool on = false;
    uint32_t changed_at = 0;

    bool observe(bool raw_on, uint32_t now) {
        if (known && raw_on == on) return false;
        if (known && !raw_on && uint32_t(now - changed_at) < 8000u)
            return false;
        known = true;
        on = raw_on;
        changed_at = now;
        return true;
    }
};

// Zero alone does not distinguish an unsupported CF field from a stopped
// fan. Require a nonzero observation in this boot before publishing zero.
struct FanFeedback {
    bool supported = false;
    bool observe(uint8_t raw) {
        if (raw != 0) supported = true;
        return supported;
    }
};

}  // namespace esphome::lg_controller
