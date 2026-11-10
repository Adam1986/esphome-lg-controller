#pragma once
#include <cstdint>

// Pure scheduling predicates used by the experimental floor console
// controller. They do not issue UART commands and can be tested independently.
namespace lg_floor_console {
inline bool readback_due(bool is_slave, uint8_t writes, uint8_t readbacks,
                         bool echo_seen, bool already_queued,
                         uint32_t elapsed_since_write, uint32_t settle_ms) {
    return is_slave && writes > 0 && writes <= 2 && readbacks < writes &&
           echo_seen && !already_queued && elapsed_since_write >= settle_ms;
}
inline bool retry_due(uint8_t writes, bool readback_queued,
                      bool write_queued, uint32_t elapsed_since_write,
                      uint32_t min_wait_ms) {
    return writes == 1 && !readback_queued && !write_queued &&
           elapsed_since_write >= min_wait_ms;
}
inline bool frame_is_newer(uint32_t first_at, uint32_t second_at) {
    return int32_t(first_at - second_at) >= 0; // rollover-safe for <2^31 ms
}
} // namespace lg_floor_console
