#pragma once
#include <cstdint>

// Pure, read-only interpretations recovered from the PREMTB100 V2.12.3a
// AC application. These functions NEVER compose or transmit control frames.
namespace lg_floorstanding {
inline bool comfort_cooling(const uint8_t *frame) { return (frame[11] & 0x04) != 0; }
inline bool supports_comfort_cooling(const uint8_t *c9) { return (c9[11] & 0x20) != 0; }
inline uint8_t discharge_code(const uint8_t *frame) { return frame[8] & 0x03; }
inline const char *discharge_name(uint8_t code) {
    switch (code) {
      case 0: return "Whole (both)";
      case 1: return "Top only";
      case 2: return "Bottom only";
      default: return "Reserved (3)";
    }
}
inline uint8_t comfort_step_code(const uint8_t *frame) { return frame[5] & 0x0F; }
inline const char *comfort_step_name(uint8_t code) {
    switch (code) {
      case 0: return "Step 1";
      case 1: return "Step 2";
      case 2: return "Step 3";
      default: return "Unknown / reserved";
    }
}
}
