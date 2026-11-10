#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../components/lg_controller/lg_slave_sleep_sync.h"
using namespace esphome::lg_controller;


void test_ceil_exact_boundary_and_countdown() {
    // Regression for user log: 419-min Master report must NEVER appear 420.
    assert(LgSlaveSleepSync::ceil_minutes(420u * 60000u) == 420);
    assert(LgSlaveSleepSync::ceil_minutes(419u * 60000u) == 419);
    assert(LgSlaveSleepSync::ceil_minutes(419u * 60000u - 1500u) == 419);
    assert(LgSlaveSleepSync::ceil_minutes(419u * 60000u - 59999u) == 419);
    assert(LgSlaveSleepSync::ceil_minutes(418u * 60000u) == 418);
    assert(LgSlaveSleepSync::ceil_minutes(1u) == 1);
    assert(LgSlaveSleepSync::ceil_minutes(0u) == 0);
}
void test_flags_no_false_16_or_override() {
    auto detector = LgSlaveSleepSync::OverrideDetector{};
    uint8_t flag[13] = {0xA8,0x12,0x04,0x1A,0x40,0x04,0x16,0x16,0x19,0x10,0x84,0,0};
    for(int i=0;i<10;i++) assert(!detector.observe(flag,420,1000,true));
    assert(detector.consistent_frames == 0);
}
void test_old_unmatched_master_does_not_abort_while_waiting() {
    auto detector = LgSlaveSleepSync::OverrideDetector{};
    uint8_t old[13] = {0xA8,0x12,0x04,0x0A,0x40,0x04,0x16,0x16,0,0,0x04,0,0};
    for(int i=0;i<10;i++) assert(!detector.observe(old,420,25000,false));
    assert(detector.consistent_frames == 0);
}
void test_after_master_match_cancel_external_overrides() {
    auto detector = LgSlaveSleepSync::OverrideDetector{};
    uint8_t cleared[13] = {0xA8,0x12,0x04,0x0A,0x40,0x04,0x16,0x16,0x18,0x00,0x04,0,0};
    assert(!detector.observe(cleared,420,30000,true));
    assert(detector.observe(cleared,420,31000,true));
    detector.reset();
    assert(!detector.observe(cleared,420,32000,true));
}
void test_after_master_match_shorter_sleep_overrides() {
    auto detector = LgSlaveSleepSync::OverrideDetector{};
    uint8_t shortsleep[13] = {0xA8,0x12,0x04,0x1A,0x40,0x04,0x16,0x16,0x18,0x3C,0x04,0,0};
    assert(!detector.observe(shortsleep,420,32000,true));
    assert(detector.observe(shortsleep,420,33200,true));
}
void test_match_resets_conflict_and_flags_dont_count() {
    auto detector = LgSlaveSleepSync::OverrideDetector{};
    uint8_t old[13] = {0xA8,0x12,0x04,0x0A,0x40,0x04,0x16,0x16,0,0,0x04,0,0};
    uint8_t good[13] = {0xA8,0x12,0x04,0x1A,0x40,0x04,0x16,0x16,0x19,0xA4,0x04,0,0};
    assert(!detector.observe(old,420,17000,true));
    assert(!detector.observe(good,420,18000,true));
    assert(detector.consistent_frames == 0);
    assert(!detector.observe(old,420,19000,true));
}
void test_cancel_pending_and_external_reset() {
    auto detector = LgSlaveSleepSync::OverrideDetector{};
    uint8_t goodclear[13] = {0xA8,0x12,0x04,0x0A,0x40,0x04,0x16,0x16,0x18,0x00,0x04,0,0};
    uint8_t newreservation[13] = {0xA8,0x12,0x04,0x1A,0x40,0x04,0x16,0x16,0x18,0x78,0x04,0,0};
    assert(!detector.observe(goodclear,0,17000,true));
    assert(!detector.observe(newreservation,0,25000,true));
    assert(detector.observe(newreservation,0,26500,true));
}
int main() {
    test_ceil_exact_boundary_and_countdown();
    test_flags_no_false_16_or_override();
    test_old_unmatched_master_does_not_abort_while_waiting();
    test_after_master_match_cancel_external_overrides();
    test_after_master_match_shorter_sleep_overrides();
    test_match_resets_conflict_and_flags_dont_count();
    test_cancel_pending_and_external_reset();
    std::puts("Slave sleep synchronisation tests passed (7 suites)");
}
