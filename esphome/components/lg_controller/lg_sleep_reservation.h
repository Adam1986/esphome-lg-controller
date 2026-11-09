#pragma once
#include <cstdint>

// Pure protocol decoder, independently tested without ESPHome.
// Status frames share a reservation-active flag in byte 3, but the payload
// at bytes 8-10 has TWO mutually exclusive encodings. In particular,
// 18.10.84 indicates a sleep-reservation FLAG, not a 16-minute duration.
namespace esphome::lg_controller {

struct LgSleepReservation {
    enum class Form : uint8_t { Other, Minutes, Flags };
    Form form = Form::Other;
    bool active = false;
    bool sleep_flag = false;
    uint16_t minutes = 0;

    static LgSleepReservation decode(const uint8_t* frame) {
        LgSleepReservation r;
        r.active = (frame[3] & 0x10) != 0;
        if (frame[10] & 0x80) {
            r.form = Form::Flags;
            r.sleep_flag = (frame[9] & 0x10) != 0;
        } else if (((frame[8] >> 3) & 0x07) == 3) {
            r.form = Form::Minutes;
            r.minutes = (uint16_t(frame[8] & 0x07) << 8) | frame[9];
        }
        return r;
    }

    static void encode_slave_request(uint8_t* frame, uint16_t minutes) {
        // The caller starts from a live genuine Master status, and is
        // responsible for recalculating the checksum after this edit.
        frame[8] = uint8_t((3u << 3) | ((minutes >> 8) & 0x07));
        frame[9] = uint8_t(minutes & 0xFF);
        frame[10] &= uint8_t(~0x80);
        if (minutes) frame[3] |= 0x10;
        else frame[3] &= uint8_t(~0x10);
    }

    // Master A8 is required to verify a requested duration. Two C8 frames
    // verifying only the reservation-active bit are NOT enough.
    static bool master_matches(uint16_t requested, const uint8_t* frame,
                               uint32_t elapsed_ms) {
        // Only the real wired Master's status frame can attest to duration.
        // A C8 unit packet can verify the reservation-active bit but not
        // establish the exact sleep duration on its own.
        if (frame[0] != 0xA8) return false;
        const auto r = decode(frame);
        if (requested == 0) {
            return !r.active && (r.form != Form::Flags || !r.sleep_flag);
        }
        if (!r.active || r.form != Form::Minutes || r.minutes == 0 || r.minutes > 480)
            return false;
        // Allow LG's countdown (ceil minutes) and the whole-minute rounding
        // used by the genuine Master, without accepting a different timer.
        const uint32_t elapsed_minutes = (elapsed_ms + 59999u) / 60000u;
        const uint32_t slack = elapsed_minutes + 2u;
        return r.minutes <= requested + 1u &&
               uint32_t(requested) <= uint32_t(r.minutes) + slack;
    }
};

}  // namespace esphome::lg_controller
