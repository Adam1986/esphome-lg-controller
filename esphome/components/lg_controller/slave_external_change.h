#pragma once

#include <cstdint>
#include <cstring>

namespace esphome::lg_controller {

// Observe ONLY verified indoor-unit status frames (C8). This detector neither
// writes to the bus nor assumes a sender can be deduced from a transient C8.
// It deliberately chooses false negatives over canceling valid ESP requests.
class SlaveExternalChangeDetector {
public:
    void arm(const uint8_t* baseline, bool valid, const uint8_t* master = nullptr, bool master_valid = false) {
        reset();
        armed_ = valid;
        if (valid) std::memcpy(baseline_, baseline, 13);
        master_baseline_valid_ = master_valid && master != nullptr;
        if (master_baseline_valid_) std::memcpy(master_baseline_, master, 13);
    }

    void reset() {
        armed_ = false;
        candidate_valid_ = false;
        candidate_reports_ = 0;
        candidate_last_millis_ = 0;
        master_baseline_valid_ = false;
        cross_c8_count_ = cross_master_count_ = 0;
        cross_c8_seen_ = cross_master_seen_ = 0;
    }

    bool observe(const uint8_t* c8, const uint8_t* expected,
                 bool matches_request, uint32_t now_ms) {
        if (!armed_) return false;
        if (matches_request) {
            // Normal progress toward our request; don't call it a takeover.
            clear_candidate_();
            cross_c8_count_ = 0;
            return false;
        }
        // Capture a *fresh changed-bit* C8 after our request. The genuine
        // Master must also subsequently attest a NEW matching state before a
        // return to the old baseline can be called an external takeover.
        if (cross_c8_count_ && now_ms - cross_c8_seen_ > 55000u)
            cross_c8_count_ = 0;
        if (cross_c8_count_ && same_operating_state_(c8, cross_c8_)) {
            if (cross_c8_count_ < 3) ++cross_c8_count_;
            cross_c8_seen_ = now_ms;
        } else if ((c8[1] & 0x01) != 0) {
            std::memcpy(cross_c8_, c8, 13);
            cross_c8_count_ = 1;
            cross_c8_seen_ = now_ms;
        }
        if (master_and_unit_agree_(now_ms)) return true;
        if (candidate_valid_) {
            if (now_ms - candidate_last_millis_ > 12000) {
                clear_candidate_();
            } else if (same_operating_state_(c8, candidate_)) {
                candidate_last_millis_ = now_ms;
                if (++candidate_reports_ >= 2) {
                    clear_candidate_();
                    return true;
                }
                return false;
            } else {
                clear_candidate_();
            }
        }
        // A fresh changed-bit status is required. Old periodic C8 frames
        // that merely disagree with an outgoing command do NOT supersede it.
        if ((c8[1] & 0x01) == 0) return false;
        // Detect a *new direction*, not an intermediate frame adopting
        // some of the components of our own request. For example, if we ask
        // for FAN_ONLY/LOW, a transient FAN_ONLY/AUTO is not ThinQ evidence.
        if (!has_independent_change_(c8, expected)) return false;
        std::memcpy(candidate_, c8, 13);
        candidate_reports_ = 1;
        candidate_last_millis_ = now_ms;
        candidate_valid_ = true;
        return false;
    }

    // The Master is authoritative. Do not treat an ordinary mismatching A8
    // as a takeover: require a Master transition since queuing, two stable
    // Master reports, AND two matching C8 reports beginning with a fresh
    // changed-bit report. This catches a LOW -> HIGH -> LOW external change
    // even when LOW equals the old C8 baseline.
    bool observe_master(const uint8_t* a8, bool matches_request, uint32_t now_ms) {
        if (!armed_ || !master_baseline_valid_) return false;
        if (matches_request || same_operating_state_(a8, master_baseline_)) {
            cross_master_count_ = 0;
            return false;
        }
        if (cross_master_count_ && now_ms - cross_master_seen_ > 55000u)
            cross_master_count_ = 0;
        if (cross_master_count_ && same_operating_state_(a8, cross_master_)) {
            if (cross_master_count_ < 3) ++cross_master_count_;
        } else {
            std::memcpy(cross_master_, a8, 13);
            cross_master_count_ = 1;
        }
        cross_master_seen_ = now_ms;
        return master_and_unit_agree_(now_ms);
    }

private:
    bool armed_ = false;
    bool candidate_valid_ = false;
    uint8_t baseline_[13] = {};
    uint8_t candidate_[13] = {};
    uint8_t candidate_reports_ = 0;
    uint32_t candidate_last_millis_ = 0;
    bool master_baseline_valid_ = false;
    uint8_t master_baseline_[13] = {};
    uint8_t cross_c8_[13] = {}, cross_master_[13] = {};
    uint8_t cross_c8_count_ = 0, cross_master_count_ = 0;
    uint32_t cross_c8_seen_ = 0, cross_master_seen_ = 0;

    bool master_and_unit_agree_(uint32_t now_ms) const {
        return master_baseline_valid_ && cross_c8_count_ >= 2 &&
               cross_master_count_ >= 2 &&
               now_ms - cross_c8_seen_ <= 55000u &&
               now_ms - cross_master_seen_ <= 55000u &&
               same_operating_state_(cross_c8_, cross_master_) &&
               !same_operating_state_(cross_master_, master_baseline_);
    }

    void clear_candidate_() {
        candidate_valid_ = false;
        candidate_reports_ = 0;
    }

    static bool same_operating_state_(const uint8_t* a, const uint8_t* b) {
        // Ignore changed flag, preheat/defrost, compressor and sensor fields.
        return ((a[1] ^ b[1]) & 0xFE) == 0 &&
               ((a[2] ^ b[2]) & 0xC4) == 0 &&
               ((a[3] ^ b[3]) & 0x10) == 0 &&
               ((a[5] ^ b[5]) & 0x01) == 0 &&
               ((a[6] ^ b[6]) & 0x0F) == 0;
    }

    static bool third_value_(uint8_t actual, uint8_t baseline,
                             uint8_t expected, uint8_t mask) {
        actual &= mask;
        baseline &= mask;
        expected &= mask;
        return actual != baseline && actual != expected;
    }

    bool has_independent_change_(const uint8_t* c8, const uint8_t* expected) const {
        // A fresh external operating state must contain a field moving to
        // something OTHER THAN its pre-request value or requested value.
        // This prevents mistaking partial adoption of our own command for
        // a conflicting ThinQ request. Power+mode, fan and setpoint are
        // evaluated independently.
        return third_value_(c8[1], baseline_[1], expected[1], 0x1E) ||
               third_value_(c8[1], baseline_[1], expected[1], 0xE0) ||
               third_value_(c8[2], baseline_[2], expected[2], 0xC4) ||
               third_value_(c8[3], baseline_[3], expected[3], 0x10) ||
               third_value_(c8[5], baseline_[5], expected[5], 0x01) ||
               third_value_(c8[6], baseline_[6], expected[6], 0x0F);
    }
};

} // namespace esphome::lg_controller
