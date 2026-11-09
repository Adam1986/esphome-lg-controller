#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::lg_controller {

// Experimental, ONE-TIME Slave status-based settings discovery.
// This does not invent a Type-B installer-settings packet. It copies a
// recently observed genuine Master status frame, changing only:
//  - sender A8 to Slave 28
//  - changed bit to 0 (not a user settings write)
//  - byte 8 bit 0x40 (unit full-settings refresh request)
// The caller recalculates the checksum; no automatic retry is permitted.
class SlavePipeTempDiscovery {
 public:
  static constexpr size_t FRAME_LEN = 13;
  static constexpr uint32_t STATUS_MAX_AGE_MS = 90u * 1000u;
  static constexpr uint32_t RESPONSE_WINDOW_MS = 90u * 1000u;

  static bool make_request(uint8_t* out, const uint8_t* master, const uint8_t* unit,
                           uint32_t master_age_ms, uint32_t unit_age_ms,
                           bool allow_active_sleep = false) {
    if (out == nullptr || master == nullptr || unit == nullptr) return false;
    if (master[0] != 0xA8 || unit[0] != 0xC8) return false;
    if (master_age_ms > STATUS_MAX_AGE_MS || unit_age_ms > STATUS_MAX_AGE_MS) return false;

    // Avoid stale or conflicting room settings: operating mode, power, fan,
    // whole- and half-degree setpoints must already agree between the genuine
    // Master and the indoor unit. Bit 0 of byte 1 is the transient changed flag.
    if ((master[1] & 0xFE) != (unit[1] & 0xFE)) return false;
    if ((master[5] & 0x01) != (unit[5] & 0x01)) return false;
    if ((master[6] & 0x0F) != (unit[6] & 0x0F)) return false;

    // Both sides must agree on the active-reservation flag. Never attempt
    // discovery during a timer transition, even with the experimental flag.
    const bool master_reserved = (master[3] & 0x10u) != 0;
    const bool unit_reserved = (unit[3] & 0x10u) != 0;
    if (master_reserved != unit_reserved) return false;
    // The genuine indoor C8 must have no independent reservation request.
    if (unit[8] != 0 || unit[9] != 0 || (unit[10] & 0x80u)) return false;

    if (master_reserved) {
      if (!allow_active_sleep) return false;
      // Even for a read request, never copy an A8 that still asserts a
      // transient settings-changed bit. Wait for a settled Master status.
      if (master[1] & 0x01u) return false;
      // Only a real Master type-3 MINUTES status is usable. The alternate
      // status-flag format (byte10 bit7), a different reservation type,
      // or an unknown duration must never be converted into a query.
      if ((master[8] & 0x38u) != 0x18u || (master[10] & 0x80u)) return false;
      const uint16_t minutes =
          (uint16_t(master[8] & 0x07u) << 8) | master[9];
      if (minutes == 0 || minutes > 480) return false;
      // No existing request/control flags in the Master header; the outgoing
      // packet must contain ONLY the full-settings query flag (byte8 bit6).
      if (master[8] & 0xC0u) return false;
    } else {
      // Original, fully tested policy for no active reservation.
      if (master[8] != 0 || master[9] != 0 || (master[10] & 0x80u))
        return false;
    }

    std::memcpy(out, master, FRAME_LEN);
    out[0] = 0x28;
    out[1] &= static_cast<uint8_t>(~0x01u); // NOT a settings write
    if (master_reserved) {
      // The PREMTB100's decoded timer fields are byte8[0..5] and byte9.
      // Clear those COMMAND fields, not the active-state bit in byte3.
      // 0x40 remains the separate full-settings READ request flag.
      // The extracted wall-controller firmware checks BOTH the change bit
      // and a nonzero timer selector before applying a reservation change.
      out[8] = 0x40;
      out[9] = 0x00;
      // Reject flags form above; leave all other Master fields unchanged.
    } else {
      out[8] |= 0x40;
    }
    // Checksum recalculated by caller. This only establishes wire-level
    // separation, not yet hardware-verified indoor-unit behaviour.
    return true;
  }
};
}  // namespace esphome::lg_controller
