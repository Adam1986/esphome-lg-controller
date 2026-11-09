#include <cassert>
#include <cstdio>
#include <cstdint>
#include "slave_external_change.h"
using esphome::lg_controller::SlaveExternalChangeDetector;

static void test_thinq_takes_over() {
    SlaveExternalChangeDetector d;
    // After own FAN_ONLY/AUTO, ESP requests FAN_ONLY/LOW. ThinQ sets HEAT/AUTO.
    uint8_t baseline[13]={0xC8,0x6A,0,2,0,4,0x97,0x1D};
    uint8_t expected[13]={0x28,0x0B,0,2,0,0x44,0x97,0x17};
    uint8_t old[13]={0xC8,0x6A,0,2,0,4,0x97,0x1D};
    uint8_t changed[13]={0xC8,0x73,0,2,0x80,4,0x97,0x1D};
    uint8_t changed2[13]={0xC8,0x73,0,0x0A,0x40,4,0x97,0x1D};
    d.arm(baseline,true);
    assert(!d.observe(old,expected,false,5000));
    assert(!d.observe(changed,expected,false,7000));
    assert(d.observe(changed2,expected,false,8500));
}
static void test_stale_and_partial_frames() {
    SlaveExternalChangeDetector d;
    uint8_t baseline[13]={0xC8,0x72,0,2,0x40,4,0x97,0x1D}; // HEAT AUTO
    uint8_t expected[13]={0x28,0x0B,0,2,0x40,0x44,0x97,0x17}; // FAN ONLY LOW
    uint8_t previous_changed[13]={0xC8,0x73,0,2,0x40,4,0x97,0x1D};
    uint8_t partial[13]={0xC8,0x6B,0,2,0x00,4,0x97,0x1D}; // FAN_ONLY/AUTO, partial progress
    uint8_t partial2[13]={0xC8,0x6A,0,0x0A,0,4,0x97,0x1D};
    d.arm(baseline,true);
    assert(!d.observe(previous_changed,expected,false,1000));
    assert(!d.observe(previous_changed,expected,false,2500));
    assert(!d.observe(partial,expected,false,3000));
    assert(!d.observe(partial2,expected,false,4500));
    uint8_t accepted[13]={0xC8,0x0A,0,2,0,4,0x97,0x1D};
    assert(!d.observe(accepted,expected,true,6000));
}
static void test_thermal_changes_not_takeover() {
    SlaveExternalChangeDetector d;
    uint8_t baseline[13]={0xC8,0x12,0,2,0x40,4,0x97,0x1F};
    uint8_t expected[13]={0x28,0x32,0,2,0x40,4,0x97,0x17};
    uint8_t preheat[13]={0xC8,0x13,0,0x0A,0xC0,0x00,0x97,0x1A};
    d.arm(baseline,true);
    assert(!d.observe(preheat,expected,false,1000));
    assert(!d.observe(preheat,expected,false,2500));
}
static void test_timeout_requires_new_changed_bit() {
    SlaveExternalChangeDetector d;
    uint8_t baseline[13]={0xC8,0x6A,0,2,0,4,0x97,0x1D};
    uint8_t expected[13]={0x28,0x0B,0,2,0,4,0x97,0x1D};
    uint8_t changed[13]={0xC8,0x73,0,2,0,4,0x97,0x1D};
    uint8_t unmarked[13]={0xC8,0x72,0,2,0,4,0x97,0x1D};
    d.arm(baseline,true);
    assert(!d.observe(changed,expected,false,1000));
    assert(!d.observe(unmarked,expected,false,15000));
    assert(!d.observe(unmarked,expected,false,16000));
    assert(!d.observe(changed,expected,false,20000));
    assert(d.observe(unmarked,expected,false,21500));
}
static void test_new_temperature_state_supersedes_fan_request() {
    SlaveExternalChangeDetector d;
    uint8_t baseline[13]={0xC8,0x12,0,2,0x40,4,0x97,0x1D};
    uint8_t expected[13]={0x28,0x32,0,2,0x40,4,0x97,0x1D};
    uint8_t changed[13]={0xC8,0x13,0,2,0x40,4,0x98,0x1D};
    uint8_t unchanged_bit[13]={0xC8,0x12,0,2,0x40,4,0x98,0x1D};
    d.arm(baseline,true);
    assert(!d.observe(changed,expected,false,1000));
    assert(d.observe(unchanged_bit,expected,false,2600));
}
int main(){test_thinq_takes_over();test_stale_and_partial_frames();test_thermal_changes_not_takeover();test_timeout_requires_new_changed_bit();test_new_temperature_state_supersedes_fan_request();puts("PASS: 5 deterministic external-control arbitration tests");}
