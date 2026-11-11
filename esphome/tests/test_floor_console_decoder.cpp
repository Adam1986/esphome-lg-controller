#include <cassert>
#include <cstdint>
#include "../components/lg_controller/floor_console_decode.h"
int main() {
 uint8_t f[13] = {};
 assert(!lg_floor_console::comfort_cooling(f));
 f[11] = 0x04; assert(lg_floor_console::comfort_cooling(f));
 f[11] = 0x20; assert(lg_floor_console::supports_comfort_cooling(f));
 f[8] = 0; assert(lg_floor_console::discharge_code(f) == 0);
 f[8] = 1; assert(lg_floor_console::discharge_code(f) == 1);
 f[8] = 2; assert(lg_floor_console::discharge_code(f) == 2);
 f[8] = 3; assert(lg_floor_console::discharge_code(f) == 3);
 f[5] = 0; assert(lg_floor_console::comfort_step_code(f)==0);
 f[5] = 1; assert(lg_floor_console::comfort_step_code(f)==1);
 f[5] = 2; assert(lg_floor_console::comfort_step_code(f)==2);
 f[5] = 0xF2; assert(lg_floor_console::comfort_step_code(f)==2);
}
