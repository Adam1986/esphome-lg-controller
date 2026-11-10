#include <cassert>
#include <cstdint>
#include <iostream>
#include "lg_slave_sleep_sync.h"
using esphome::lg_controller::LgSlaveSleepSync;
int main() {
  using Guard = LgSlaveSleepSync::PowerState;
  using Change = Guard::Change;
  Guard p;
  uint8_t m[13]={0xA8,0x12,0,0x1A,0,0,0,0,0x19,0xA4,4,0,0};
  uint8_t u[13]={0xC8,0x12,0,0x1A,0,0,0,0,0,0,0,0,0};
  assert(p.observe(m)==Change::None && !p.off_latched);
  assert(p.observe(u)==Change::None && !p.off_latched);
  // First indoor-unit OFF packet doesn't count as a confirmed OFF.
  u[1]=0x10;
  assert(p.observe(u)==Change::None && !p.off_latched);
  assert(p.observe(u)==Change::TurnedOff && p.off_latched);
  // Master ON by itself is not sufficient to unlock a new timer.
  assert(p.observe(m)==Change::None && p.off_latched);
  assert(p.observe(m)==Change::None && p.off_latched);
  // Two parties ON unlock new reservations.
  u[1]=0x12;
  assert(p.observe(u)==Change::TurnedOn && !p.off_latched);
  // Master OFF must immediately override even a still-active reservation bit.
  m[1]=0x10;
  assert(p.observe(m)==Change::TurnedOff && p.off_latched);
  assert(p.observe(m)==Change::None && p.off_latched);
  // An old indoor ON isn't enough to erase OFF until Master is ON too.
  assert(p.observe(u)==Change::None && p.off_latched);
  m[1]=0x12;
  assert(p.observe(m)==Change::TurnedOn && !p.off_latched);
  // A single stray indoor OFF is reset by an ON before the second OFF.
  u[1]=0x10;
  assert(p.observe(u)==Change::None && !p.off_latched);
  u[1]=0x12;
  assert(p.observe(u)==Change::None && !p.off_latched);
  u[1]=0x10;
  assert(p.observe(u)==Change::None && !p.off_latched);
  u[1]=0x12;
  assert(p.observe(u)==Change::None && !p.off_latched);
  // No effect from other message types.
  uint8_t flags[13]={0x28,0x10,0,0,0,0,0,0,0,0,0,0,0};
  assert(p.observe(flags)==Change::None);
  std::cout << "LG Slave sleep power-off sync: 16 checks passed\n";
}
