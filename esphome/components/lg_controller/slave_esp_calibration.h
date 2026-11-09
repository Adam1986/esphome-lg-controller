#pragma once
#include <cstdint>
#include <cstring>
namespace esphome::lg_controller {
// Generate a single-field installer ESP calibration write from genuine indoor CA.
// No direct UART operation; caller must recalculate checksum and check readback.
struct SlaveEspCalibration {
    static bool make_write(uint8_t* out, const uint8_t* ca, int index, int value) {
        if (out == nullptr || ca == nullptr || ca[0] != 0xCA ||
            index < 0 || index > 3 || value < 0 || value > 255) return false;
        std::memcpy(out, ca, 13);
        out[0] = 0x2A;
        out[2 + index] = uint8_t(value);
        return true;
    }
};
}  // namespace esphome::lg_controller
