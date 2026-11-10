#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "../components/lg_controller/lg_capability_refresh.h"
using esphome::lg_controller::LgCapabilityRefresh;
using Result = LgCapabilityRefresh::Result;

static void test_initial_discovery_without_restart() {
  LgCapabilityRefresh f;
  uint8_t stored[13]{};
  const uint8_t packet[13]{0xC9,0x12,0xEB,0x5D,0x06,0x3C,0x20,0x84,0xFF,0x97,0xEE,0x83,0x45};
  assert(f.observe(packet,stored)==Result::WAITING_FOR_REPEAT);
  assert(stored[0]==0);
  assert(f.observe(packet,stored)==Result::ACCEPTED_WITH_TRAIT_CHANGE);
  assert(std::memcmp(stored,packet,13)==0);
  assert(f.observe(packet,stored)==Result::UNCHANGED);
}
static void test_changed_unknown_bits_dont_refresh_ha() {
  LgCapabilityRefresh f;
  uint8_t stored[13]{0xC9,0x12,0xEB,0x5D,0x06,0x3C,0x20,0x84,0xFF,0x97,0xEE,0x83,0x45};
  uint8_t updated[13]{};
  std::memcpy(updated,stored,13);
  updated[8]^=0x01;
  assert(f.observe(updated,stored)==Result::WAITING_FOR_REPEAT);
  assert(f.observe(updated,stored)==Result::ACCEPTED_WITHOUT_TRAIT_CHANGE);
  assert(stored[8]==(uint8_t)(0xFF ^ 0x01));
}
static void test_mode_fan_swing_changes_refresh_ha() {
  for(int i=0;i<3;++i){
    LgCapabilityRefresh f;
    uint8_t stored[13]{0xC9,0x12,0xEB,0x5D,0x06,0x3C,0x20,0x84,0xFF,0x97,0xEE,0x83,0x45};
    uint8_t updated[13]{}; std::memcpy(updated,stored,13);
    const int ix=i==0?1:i==1?2:3;
    updated[ix]^= i==0?0x40:i==1?0x40:0x02;
    assert(f.observe(updated,stored)==Result::WAITING_FOR_REPEAT);
    assert(f.observe(updated,stored)==Result::ACCEPTED_WITH_TRAIT_CHANGE);
  }
}
static void test_interleaved_candidates_not_accepted() {
  LgCapabilityRefresh f;
  uint8_t stored[13]{};
  uint8_t a[13]{0xC9,0x12,0xEB,0x5D};
  uint8_t b[13]{0xC9,0x13,0xEB,0x5D};
  assert(f.observe(a,stored)==Result::WAITING_FOR_REPEAT);
  assert(f.observe(b,stored)==Result::WAITING_FOR_REPEAT);
  assert(f.observe(a,stored)==Result::WAITING_FOR_REPEAT);
  assert(stored[0]==0);
  assert(f.observe(a,stored)==Result::ACCEPTED_WITH_TRAIT_CHANGE);
}
static void test_return_to_saved_resets_candidate() {
  LgCapabilityRefresh f;
  uint8_t stored[13]{0xC9,0x12,0xEB,0x5D};
  uint8_t change[13]{0xC9,0x92,0xEB,0x5D};
  assert(f.observe(change,stored)==Result::WAITING_FOR_REPEAT);
  assert(f.observe(stored,stored)==Result::UNCHANGED);
  assert(f.observe(change,stored)==Result::WAITING_FOR_REPEAT);
  assert(f.observe(change,stored)==Result::ACCEPTED_WITH_TRAIT_CHANGE);
}
int main(){
  test_initial_discovery_without_restart();
  test_changed_unknown_bits_dont_refresh_ha();
  test_mode_fan_swing_changes_refresh_ha();
  test_interleaved_candidates_not_accepted();
  test_return_to_saved_resets_candidate();
  puts("PASS C9 live capabilities tests");
}
