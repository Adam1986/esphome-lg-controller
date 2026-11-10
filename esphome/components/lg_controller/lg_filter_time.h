#pragma once
#include <cstdint>

// Type-4 indoor unit CC: byte 1 contains the low 8 bits of remaining filter
// hours, and the LOW nibble of byte 2 contains the high 4 bits. The upper
// nibble contains unrelated occupancy/dual-setpoint indicators.
// Confirm two consistent real indoor CC reports; a Master AC (or UART echo)
// is NOT used as proof of the indoor unit's remaining filter life.
namespace esphome::lg_controller {
class LgFilterTimeRemaining {
public:
    static uint16_t decode(const uint8_t* frame) {
        return uint16_t(frame[1]) | (uint16_t(frame[2] & 0x0f) << 8);
    }

    bool observe_indoor_cc(const uint8_t* frame) {
        if (frame[0] != 0xCC) return false;
        const uint16_t hours = decode(frame);
        if (candidate_count_ && candidate_hours_ == hours) {
            if (candidate_count_ < 2) ++candidate_count_;
        } else {
            candidate_hours_ = hours;
            candidate_count_ = 1;
        }
        if (candidate_count_ < 2) return false;
        if (!confirmed_ || confirmed_hours_ != hours) {
            confirmed_ = true;
            confirmed_hours_ = hours;
            return true;
        }
        return false;
    }

    bool confirmed() const { return confirmed_; }
    uint16_t hours() const { return confirmed_hours_; }

private:
    bool confirmed_ = false;
    uint16_t confirmed_hours_ = 0;
    uint16_t candidate_hours_ = 0;
    uint8_t candidate_count_ = 0;
};
} // namespace esphome::lg_controller
