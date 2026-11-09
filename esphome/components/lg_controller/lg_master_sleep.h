#pragma once
#include <cstdint>

namespace esphome::lg_controller {
// Master sleep command: type-3, 11-bit minutes (0..480). Cancellation is
// transmitted explicitly as type-3 with a zero-minute duration, rather than
// silently omitting the timer type on the first cancel frame.
class LgMasterSleep {
public:
    static void encode(uint8_t* frame, uint16_t minutes) {
        if (minutes > 480) minutes = 480;
        frame[3] = (frame[3] & uint8_t(~0x10u)) | (minutes ? 0x10 : 0);
        frame[8] = uint8_t(0x18u | ((minutes >> 8) & 0x07u));
        frame[9] = uint8_t(minutes & 0xffu);
        // A C8 template may contain the flag-mask discriminator. In a Master
        // Type-3 minutes message it MUST be cleared or byte 9 is reinterpreted
        // as reservation flags (the infamous 18.10.84 ambiguity).
        frame[10] &= uint8_t(~0x80u);
    }
};
} // namespace esphome::lg_controller
