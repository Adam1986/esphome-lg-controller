#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../components/lg_controller/lg_vane_mapping.h"
using esphome::lg_controller::LgVaneMapping;
int main() {
    for (unsigned raw = 0; raw < 7; ++raw) {
        for (bool reversed : {false, true}) {
            unsigned ui = LgVaneMapping::wire_to_option(raw, reversed);
            assert(ui < 7);
            assert(LgVaneMapping::option_to_wire(ui, reversed) == raw);
            assert(LgVaneMapping::wire_to_option(LgVaneMapping::option_to_wire(raw, reversed), reversed) == raw);
        }
    }
    assert(LgVaneMapping::option_to_wire(0, true) == 6); // '6 (Up)'
    assert(LgVaneMapping::option_to_wire(5, true) == 1); // '1 (Down)'
    assert(LgVaneMapping::option_to_wire(6, true) == 0); // '0 (Default)'
    assert(LgVaneMapping::option_to_wire(0, false) == 0); // standard
    puts("PASS: floor console labels and raw vane roundtrip for all 4 vanes");
}
