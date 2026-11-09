#include <cassert>
#include <cstdio>
#include "../components/lg_controller/lg_master_sleep.h"
#include "../components/lg_controller/lg_sleep_reservation.h"
#include "../components/lg_controller/lg_slave_sleep_sync.h"
using esphome::lg_controller::LgMasterSleep;
using esphome::lg_controller::LgSleepReservation;
using esphome::lg_controller::LgSlaveSleepSync;
int main() {
    uint8_t f[13] = {0xA8, 0x53, 0xC4, 0x02, 0x40,0,0,0,0,0,0x84,0,0};
    LgMasterSleep::encode(f, 2);
    assert((f[3]&0x10)!=0 && f[8]==0x18 && f[9]==2 && f[10]==0x04);
    auto r=LgSleepReservation::decode(f);
    assert(r.active && r.form==LgSleepReservation::Form::Minutes && r.minutes==2);
    LgMasterSleep::encode(f, 480);
    assert(f[8]==0x19 && f[9]==0xE0);
    LgMasterSleep::encode(f, 0);
    assert(!(f[3]&0x10) && f[8]==0x18 && f[9]==0);
    r=LgSleepReservation::decode(f);
    assert(!r.active && r.form==LgSleepReservation::Form::Minutes && r.minutes==0);
    assert(LgSlaveSleepSync::ceil_minutes(120000)==2);
    assert(LgSlaveSleepSync::ceil_minutes(119999)==2);
    assert(LgSlaveSleepSync::ceil_minutes(60000)==1);
    assert(LgSlaveSleepSync::ceil_minutes(59999)==1);
    puts("PASS: Master sleep 2/480 minutes, exact ceiling, explicit type-3 zero cancel");
}
