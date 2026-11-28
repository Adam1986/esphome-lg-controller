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

// CF byte 11 is empirical fan feedback, not a direct Thermo ON/OFF bit.
// Zero alone cannot distinguish an unsupported field from a stopped fan.
// Learn support only from nonzero indoor feedback in this boot.
struct FanFeedback {
    static constexpr uint32_t MAX_AGE_MS = 180000u;
    bool supported = false;
    bool heating_candidate = false;
    bool sample_valid = false;
    uint8_t raw = 0;
    uint32_t sampled_at = 0;

    bool observe(uint8_t value, uint32_t now) {
        if (value != 0) supported = true;
        raw = value;
        sampled_at = now;
        sample_valid = supported;
        return supported;
    }

    // Never carry a stopped/running sample across an observed change into
    // or out of active heating (OFF, preheat, defrost, other modes, etc.).
    void set_heating_candidate(bool candidate) {
        if (candidate != heating_candidate) sample_valid = false;
        heating_candidate = candidate;
    }

    bool stopped(uint32_t now) const {
        return heating_candidate && supported && sample_valid && raw == 0 &&
               uint32_t(now - sampled_at) < MAX_AGE_MS;
    }

    bool expire(uint32_t now) {
        if (!sample_valid || uint32_t(now - sampled_at) < MAX_AGE_MS)
            return false;
        sample_valid = false;
        return true;
    }
};

}  // namespace esphome::lg_controller
