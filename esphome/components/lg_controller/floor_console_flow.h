#pragma once
#include <cstdint>
#include "floor_console_wire.h"

// Pure scheduling predicates used by the experimental floor console
// controller. They do not issue UART commands and can be tested independently.
namespace lg_floor_console {
// No speculative Type-D read opcode: give an optional passive observation
// window ONLY to discharge-direction changes on Slave floor consoles.
// The existing guarded full-settings read remains the bounded fallback.
inline uint32_t readback_settle_ms(bool slave, Page page, bool passive_enabled,
                                   uint32_t regular_ms, uint32_t passive_ms) {
    return slave && passive_enabled && page == Page::Discharge
        ? passive_ms : regular_ms;
}

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
// Permit a single guarded read immediately, then at most one bounded retry.
// No read request becomes a page write unless a genuine page is received.
inline bool template_refresh_due(uint8_t attempts, uint32_t elapsed_since_last,
                                 uint32_t retry_after_ms, uint8_t maximum_attempts) {
    return attempts < maximum_attempts &&
           (attempts == 0 || elapsed_since_last >= retry_after_ms);
}
inline bool frame_is_newer(uint32_t first_at, uint32_t second_at) {
    return int32_t(first_at - second_at) >= 0; // rollover-safe for <2^31 ms
}

// V2.0.6: optionally accept a single *fresh matching* genuine indoor CD.
// Keep two consistent mismatches before declaring a rejection/override.
// Callers must enforce post-readback freshness, source and UART-echo checks.
enum class FastCDResult : uint8_t { Waiting, Confirmed, Different };
struct FastCDVerifier {
    uint8_t target = 0;
    uint8_t matching = 0;
    uint8_t differing = 0;
    uint8_t last_different = 0xFF;

    void reset(uint8_t requested) {
        target = requested;
        matching = differing = 0;
        last_different = 0xFF;
    }
    FastCDResult observe(uint8_t actual, bool allow_single_matching_cd = false) {
        if (actual == target) {
            differing = 0;
            last_different = 0xFF;
            if (matching < 2) ++matching;
            return (allow_single_matching_cd || matching >= 2)
                ? FastCDResult::Confirmed : FastCDResult::Waiting;
        }
        matching = 0;
        if (last_different != actual) {
            last_different = actual;
            differing = 0;
        }
        if (differing < 2) ++differing;
        return differing >= 2 ? FastCDResult::Different : FastCDResult::Waiting;
    }
};
// A recent request must not be overwritten by an unsolicited (or a stale
// already-in-flight) indoor CD. Only a genuine post-query CD after the readback
// echo may resolve the current optimistic request.
inline bool suppress_unverified_discharge_cd(bool optimistic_request_pending,
                                             bool verification_pending,
                                             bool verification_sent,
                                             bool verification_echo_seen) {
    return optimistic_request_pending &&
           !(verification_pending && verification_sent && verification_echo_seen);
}

// The loop snapshots its clock *before* handling UART RX. An echo handler may
// start a verification timer slightly later in the same iteration; unsigned
// subtraction would otherwise underflow and look ~49 days old.
// Signed deltas still handle ordinary millis() rollover for sub-24-day timers.
inline uint32_t fast_elapsed_ms(uint32_t now, uint32_t started_at) {
    const int32_t elapsed = static_cast<int32_t>(now - started_at);
    return elapsed < 0 ? 0u : static_cast<uint32_t>(elapsed);
}

inline bool fast_verify_due(bool pending, bool sent, bool tx_active, bool deferred,
                            uint32_t elapsed_quiet, uint32_t quiet_ms) {
    return pending && !sent && !tx_active && !deferred && elapsed_quiet >= quiet_ms;
}
// A quiet-clock reset on subsequent user activity must never extend the
// *absolute* verification deadline. Once sent, use the fixed CD reply timer.
inline bool fast_verify_expired(bool sent, uint32_t elapsed_absolute,
                                uint32_t quiet_ms, uint32_t send_allowance_ms,
                                uint32_t response_timeout_ms) {
    return elapsed_absolute >= (sent ? response_timeout_ms : quiet_ms + send_allowance_ms);
}

} // namespace lg_floor_console
