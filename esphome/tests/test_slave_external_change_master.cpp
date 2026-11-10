#include <cassert>
#include <cstdio>
#include <cstdint>
#include "slave_external_change.h"
using esphome::lg_controller::SlaveExternalChangeDetector;
static void test_garage_log_baseline_reversion() {
  SlaveExternalChangeDetector d;
  uint8_t last_c8[13]={0xC8,0x0A,0,2,0,4,0x11,9}; // LOW
  uint8_t master_high[13]={0xA8,0x4A,0,2,0,4,0x11,9};
  uint8_t expected[13]={0x28,0x2B,0,2,0,4,0x11,9}; // MED
  uint8_t changed_low[13]={0xC8,0x0B,0,2,0,4,0x11,9};
  uint8_t master_low[13]={0xA8,0x0A,0,2,0,4,0x11,9};
  d.arm(last_c8,true,master_high,true);
  assert(!d.observe(changed_low,expected,false,1000));
  assert(!d.observe(changed_low,expected,false,2500));
  assert(!d.observe_master(master_low,false,14000));
  assert(d.observe_master(master_low,false,15500));
}
static void test_slow_response_stays_pending() {
  SlaveExternalChangeDetector d;
  uint8_t low[13]={0xC8,0x0A,0,2,0,4,0x11,9};
  uint8_t ml[13]={0xA8,0x0A,0,2,0,4,0x11,9};
  uint8_t med[13]={0x28,0x2B,0,2,0,4,0x11,9};
  uint8_t marked_old[13]={0xC8,0x0B,0,2,0,4,0x11,9};
  d.arm(low,true,ml,true);
  assert(!d.observe(marked_old,med,false,2000));
  assert(!d.observe(low,med,false,3000));
  assert(!d.observe_master(ml,false,14000));
  assert(!d.observe_master(ml,false,15000));
}
static void test_master_only_is_not_enough() {
  SlaveExternalChangeDetector d;
  uint8_t low[13]={0xC8,0x0A,0,2,0,4,0x11,9};
  uint8_t mh[13]={0xA8,0x4A,0,2,0,4,0x11,9};
  uint8_t ml[13]={0xA8,0x0A,0,2,0,4,0x11,9};
  d.arm(low,true,mh,true);
  assert(!d.observe_master(ml,false,14000));
  assert(!d.observe_master(ml,false,15500));
}
int main(){test_garage_log_baseline_reversion();test_slow_response_stays_pending();test_master_only_is_not_enough();puts("PASS: Master+IDU pending external control arbitration");}
