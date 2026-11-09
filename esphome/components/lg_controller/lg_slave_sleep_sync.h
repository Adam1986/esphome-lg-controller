#pragma once
#include <cstdint>
#include "lg_sleep_reservation.h"

// Independent Slave sleep-time display/override rules. A real Master A8
// duration is authoritative; flag-format A8 messages are never durations.
namespace esphome::lg_controller {

struct LgSlaveSleepSync {
    // Ceil remaining time correctly: exactly 419:00 means 419, NOT 420.
    // This does not alter the original Master-mode countdown behaviour.
    static uint32_t ceil_minutes(uint32_t remaining_ms) {
        return remaining_ms / 60000u + (remaining_ms % 60000u != 0 ? 1u : 0u);
    }

    // V1.8.3: The genuine LG system clears a sleep reservation whenever the
    // unit is powered OFF. A8/C8 can briefly interleave old ON and new OFF
    // packets, so keep HA at zero until BOTH Master A8 and indoor C8 have
    // subsequently reported power ON. Only accept an indoor-unit OFF after
    // two consecutive C8 OFF reports to reject a stray packet.
    struct PowerState {
        enum class Change : uint8_t { None, TurnedOff, TurnedOn };
        bool off_latched = false;
        uint8_t on_seen_mask = 0;
        uint8_t unit_off_streak = 0;

        Change observe(const uint8_t* frame) {
            const bool master = frame[0] == 0xA8;
            const bool unit = frame[0] == 0xC8;
            if (!master && !unit) return Change::None;
            const bool on = (frame[1] & 0x02) != 0;
            if (!on) {
                if (unit && unit_off_streak < 2) ++unit_off_streak;
                if (master || unit_off_streak >= 2) {
                    on_seen_mask = 0;
                    if (!off_latched) {
                        off_latched = true;
                        return Change::TurnedOff;
                    }
                }
                return Change::None;
            }
            if (unit) unit_off_streak = 0;
            if (off_latched) {
                on_seen_mask |= master ? 0x01 : 0x02;
                if (on_seen_mask == 0x03) {
                    off_latched = false;
                    on_seen_mask = 0;
                    return Change::TurnedOn;
                }
            }
            return Change::None;
        }
    };

    // Slave-only: never transmit a positive sleep reservation while the
    // genuine controller and indoor unit have not BOTH confirmed power ON.
    // Merely showing ON in Home Assistant is insufficient during transitions.
    struct SleepSetGuard {
        static bool can_set(bool have_master, uint8_t master_byte_1,
                            bool have_indoor, uint8_t indoor_byte_1,
                            bool local_mode_on, bool off_latched) {
            constexpr uint8_t POWER_ON_BIT = 0x02;
            return have_master && have_indoor && local_mode_on && !off_latched &&
                   (master_byte_1 & POWER_ON_BIT) != 0 &&
                   (indoor_byte_1 & POWER_ON_BIT) != 0;
        }
    };

    // Only count explicit, contradictory Master states; never treat the
    // 18.10.84 "sleep enabled" flag as a contradictory duration.
    struct OverrideDetector {
        uint16_t last_minutes = 0;
        bool last_active = false;
        uint8_t consistent_frames = 0;

        void reset() {
            last_minutes = 0;
            last_active = false;
            consistent_frames = 0;
        }

        // Precondition: the Master already matched our outgoing request once.
        // Before that, old A8 statuses are expected and must NOT abort it.
        bool observe(const uint8_t *frame, uint16_t requested,
                     uint32_t elapsed_ms, bool master_ever_confirmed) {
            if (!master_ever_confirmed || frame[0] != 0xA8) {
                reset();
                return false;
            }
            const auto r = LgSleepReservation::decode(frame);
            if (LgSleepReservation::master_matches(requested, frame, elapsed_ms)) {
                reset();
                return false;
            }
            // Active sleep flag (0x10) does not encode minutes and cannot
            // contradict a pending set. Likewise, an unrelated reservation
            // message does not prove that sleep was modified externally.
            if (r.active && ((r.form == LgSleepReservation::Form::Flags && r.sleep_flag) ||
                             r.form == LgSleepReservation::Form::Other)) {
                return false;
            }
            const bool active = r.active && r.form == LgSleepReservation::Form::Minutes &&
                                r.minutes > 0 && r.minutes <= 480;
            const uint16_t minutes = active ? r.minutes : 0;
            // If active flag-form explicitly says sleep is off, that's a
            // contradictory state too. Unknown/malformed minute values are
            // not grounds for cancelling a queued user command.
            if (r.active && r.form == LgSleepReservation::Form::Minutes && !active)
                return false;
            if (consistent_frames == 0 || active != last_active || minutes != last_minutes) {
                last_active = active;
                last_minutes = minutes;
                consistent_frames = 1;
            } else if (consistent_frames < 2) {
                ++consistent_frames;
            }
            return consistent_frames >= 2;
        }
    };
};

}  // namespace esphome::lg_controller
