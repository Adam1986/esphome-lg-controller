#pragma once
#include <cstdint>

namespace esphome::lg_controller {
// The LG wire format always stores raw vane codes 0..6. Only the
// floor-standing HA option order is different. Never change the payload.
struct LgVaneMapping {
    static uint8_t option_to_wire(uint8_t option_index, bool reversed) {
        return reversed && option_index <= 6 ? 6 - option_index : option_index;
    }
    static uint8_t wire_to_option(uint8_t wire_value, bool reversed) {
        return reversed && wire_value <= 6 ? 6 - wire_value : wire_value;
    }
};
} // namespace esphome::lg_controller
