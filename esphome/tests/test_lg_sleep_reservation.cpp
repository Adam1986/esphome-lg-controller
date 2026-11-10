#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../components/lg_controller/lg_sleep_reservation.h"
using esphome::lg_controller::LgSleepReservation;

static void test_master_240_min_and_flag_16_not_duration() {
    uint8_t duration[13] = {0xA8,0x73,0x00,0x12,0x40,0x44,0x99,0x17,0x18,0xF0,0x04,0x00,0};
    auto a = LgSleepReservation::decode(duration);
    assert(a.active && a.form == LgSleepReservation::Form::Minutes && a.minutes == 240);
    assert(LgSleepReservation::master_matches(240, duration, 0));
    duration[0] = 0xC8;
    assert(!LgSleepReservation::master_matches(240, duration, 0));
    duration[0] = 0xA8;
    assert(!LgSleepReservation::master_matches(60, duration, 0));
    duration[9] = 0x10; duration[10] = 0x84;
    auto f = LgSleepReservation::decode(duration);
    assert(f.active && f.form == LgSleepReservation::Form::Flags && f.sleep_flag);
    assert(f.minutes == 0); // THIS MUST NEVER PUBLISH 16 MINUTES
    assert(!LgSleepReservation::master_matches(16, duration, 0));
    assert(!LgSleepReservation::master_matches(240, duration, 0));
}
static void test_master_remaining_counts_down() {
    uint8_t duration[13] = {0xA8,0x73,0,0x12,0,0,0,0,0x18,0xEE,0x04,0,0};
    assert(LgSleepReservation::master_matches(240,duration,120000)); // 238 min after 2 min
    duration[9]=0xD0;
    assert(!LgSleepReservation::master_matches(240,duration,120000));
}
static void test_encode_set_cancel_480_and_unchanged_fields() {
    uint8_t before[13] = {0x28,0x72,0x04,0x02,0x40,0x05,0x16,0x17,0x00,0x00,0x84,0x00,0xC3};
    uint8_t frame[13] = {};
    std::memcpy(frame,before,sizeof(frame));
    LgSleepReservation::encode_slave_request(frame,240);
    assert(frame[8]==0x18 && frame[9]==0xF0 && frame[10]==0x04 && (frame[3]&0x10));
    for (int i : {0,1,2,4,5,6,7,11,12}) assert(frame[i]==before[i]);
    std::memcpy(frame,before,sizeof(frame));
    LgSleepReservation::encode_slave_request(frame,480);
    assert(frame[8]==0x19 && frame[9]==0xE0 && frame[10]==0x04 && (frame[3]&0x10));
    std::memcpy(frame,before,sizeof(frame));
    frame[3]|=0x10;
    LgSleepReservation::encode_slave_request(frame,0);
    assert(frame[8]==0x18 && frame[9]==0 && frame[10]==0x04 && !(frame[3]&0x10));
}
static void test_cancel_requires_master_inactive() {
    uint8_t a8[13] = {0xA8,0x72,0,0x02,0,0,0,0,0,0,0x04,0,0};
    assert(LgSleepReservation::master_matches(0,a8,1000));
    a8[3]=0x12;
    assert(!LgSleepReservation::master_matches(0,a8,1000));
    a8[3]=0x02; a8[8]=0x18; a8[9]=0x10; a8[10]=0x84;
    assert(!LgSleepReservation::master_matches(0,a8,1000));
    a8[9]=0;
    assert(LgSleepReservation::master_matches(0,a8,1000));
}
static void test_other_reservation_not_sleep_minutes() {
    uint8_t a8[13] = {0xA8,0x73,0,0x12,0,0,0,0,0x10,0xF0,0x04,0,0};
    auto r = LgSleepReservation::decode(a8);
    assert(r.form == LgSleepReservation::Form::Other);
    assert(!LgSleepReservation::master_matches(240,a8,0));
}
int main() {
    test_master_240_min_and_flag_16_not_duration();
    test_master_remaining_counts_down();
    test_encode_set_cancel_480_and_unchanged_fields();
    test_cancel_requires_master_inactive();
    test_other_reservation_not_sleep_minutes();
    std::puts("Sleep reservation tests passed (5 suites)");
}
