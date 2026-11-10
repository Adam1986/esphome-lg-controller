#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../components/lg_controller/slave_pipe_temp_discovery.h"
using esphome::lg_controller::SlavePipeTempDiscovery;

static uint8_t checksum(const uint8_t *x) {
  unsigned sum = 0;
  for (int i=0; i<12; ++i) sum += x[i];
  return static_cast<uint8_t>(sum & 0xff) ^ 0x55;
}

int main() {
  // Real Lounge Master and indoor frames from 2026-11-08 22:11 (420 minute sleep).
  uint8_t master[13] = {0xA8,0x12,0x00,0x12,0x40,0x45,0xD7,0x19,0x19,0xA4,0x04,0x00,0x57};
  uint8_t unit[13] = {0xC8,0x12,0x00,0x12,0x40,0x05,0xD7,0x1E,0x00,0x00,0x00,0x00,0x73};
  uint8_t out[13] = {};
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000)); // default STILL safe
  assert(SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  assert(out[0]==0x28 && out[1]==0x12 && out[3]==0x12);
  assert(out[8]==0x40 && out[9]==0 && out[10]==0x04);
  assert((out[1]&1u)==0 && (out[8]&0x38u)==0 && (out[8]&0x07u)==0);
  for (int i=2; i<=11; ++i) if (i!=8 && i!=9) assert(out[i]==master[i]);
  out[12]=checksum(out);
  printf("Live Lounge 420m Master A8 -> experimental diagnostic query: ");
  for (auto b:out) printf("%02X ",b);
  puts("");
  assert(out[12]==checksum(out));
  // Alternative A8 form carries sleep marker in byte10: must not use it.
  master[9]=0x10; master[10]=0x84;
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  master[9]=0xA4; master[10]=0x04;
  // Never query during Master/indoor disagreement or stale data.
  unit[3]=0x02;
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  unit[3]=0x12;
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,90001,1000,true));
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,90001,true));
  master[1]=0x13;
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  // Wait for Master to settle; NEVER mask over a new in-flight change.
  master[1]=0x12;
  master[8]=0x11; // another timer type, not sleep
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  master[8]=0x18; master[9]=0; // invalid zero duration
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  master[8]=0x19; master[9]=0xE1; // 481 minutes invalid
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  master[8]=0x19; master[9]=0xA4;
  unit[8]=0x40;
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  unit[8]=0;
  unit[1]=0x22;
  assert(!SlavePipeTempDiscovery::make_request(out,master,unit,1000,1000,true));
  puts("Experimental active-sleep diagnostic request: 13 checks passed");
}
