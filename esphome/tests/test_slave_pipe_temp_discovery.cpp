#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "../components/lg_controller/slave_pipe_temp_discovery.h"
using esphome::lg_controller::SlavePipeTempDiscovery;

static void test_living_area_known_status() {
  const uint8_t master[13] = {0xA8,0x72,0x04,0x02,0x40,0x04,0x17,0x17,0x00,0x00,0x04,0x00,0xC3};
  const uint8_t unit[13]   = {0xC8,0x72,0x04,0x02,0x40,0x04,0x17,0x17,0x00,0x00,0x00,0x00,0xE7};
  uint8_t out[13] = {};
  assert(SlavePipeTempDiscovery::make_request(out,master,unit,3500,3500));
  assert(out[0]==0x28 && out[1]==0x72 && out[8]==0x40);
  for (unsigned i=1;i<12;i++) { if (i==8) continue; assert(out[i]==master[i]); }
  assert(master[0]==0xA8 && master[8]==0x00);
}
static void test_lounge_source_preserves_thermistor_and_room_sensor() {
  const uint8_t master[13] = {0xA8,0x72,0x00,0x02,0x40,0x44,0xD6,0x16,0x00,0x00,0x04,0x00,0xC5};
  const uint8_t unit[13]   = {0xC8,0x72,0x00,0x02,0x40,0x04,0xD6,0x1C,0x00,0x00,0x00,0x00,0x27};
  uint8_t out[13];
  assert(SlavePipeTempDiscovery::make_request(out,master,unit,6000,3000));
  assert(out[5]==0x44 && out[6]==0xD6 && out[7]==0x16); // Master authority unchanged
}
static void test_changed_bit_is_cleared() {
  uint8_t master[13] = {0xA8,0x73,0,0x02,0x40,0x44,0xD6,0x16};
  uint8_t unit[13] = {0xC8,0x72,0,0x02,0x40,0x04,0xD6,0x1C};
  uint8_t out[13];
  assert(SlavePipeTempDiscovery::make_request(out,master,unit,0,0));
  assert(out[1]==0x72);
}
static void test_reject_stale_or_disagreeing_or_reserved() {
  uint8_t master[13] = {0xA8,0x72,0,0x02,0x40,0x44,0xD6,0x16};
  uint8_t unit[13] = {0xC8,0x72,0,0x02,0x40,0x04,0xD6,0x1C};
  uint8_t out[13];
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,90001,0));
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,90001));
  unit[1]=0x12; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); unit[1]=0x72;
  unit[6]=0xD7; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); unit[6]=0xD6;
  unit[5]=0x05; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); unit[5]=0x04;
  master[3]=0x12; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); master[3]=0x02;
  unit[3]=0x12; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); unit[3]=0x02;
  master[8]=0x18; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); master[8]=0;
  master[9]=0x10; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); master[9]=0;
  unit[9]=0x10; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); unit[9]=0;
  master[10]=0x84; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); master[10]=0x04;
  master[0]=0x28; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0)); master[0]=0xA8;
  unit[0]=0xA8; assert(!SlavePipeTempDiscovery::make_request(out,master,unit,0,0));
}
int main() {
  test_living_area_known_status(); test_lounge_source_preserves_thermistor_and_room_sensor();
  test_changed_bit_is_cleared(); test_reject_stale_or_disagreeing_or_reserved();
  std::puts("4 Slave full-settings discovery policy tests passed");
}
