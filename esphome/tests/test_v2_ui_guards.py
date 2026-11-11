"""Compile the *actual* LgSwitch/LgSelect class code against small API stubs.

This validates pre-CA command rejection and makes sure genuine indoor readback
via publish_state bypasses the user-write guard. Requires a C++17 compiler.
This is not a complete ESPHome build.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
text = (root / "components/lg_controller/lg-controller.h").read_text()
start = text.index("class LgSwitch final")
end = text.index("class LgNumber final", start)
real_classes = text[start:end]

source = r'''
#include <cassert>
#include <functional>
#include <optional>
#include <string>
namespace esphome { namespace switch_ {
enum SwitchRestoreMode { SWITCH_RESTORE_DEFAULT_OFF };
class Switch {
public:
    bool state = false;
    int publishes = 0;
    virtual ~Switch() = default;
    void apply_user_request(bool value) { write_state(value); }
    void publish_state(bool value) { state = value; ++publishes; }
protected:
    virtual void write_state(bool) = 0;
    void set_restore_mode(SwitchRestoreMode) {}
    std::optional<bool> get_initial_state_with_restore_mode() { return std::nullopt; }
};
}
namespace select {
class Select {
public:
    std::string state;
    int publishes = 0;
    virtual ~Select() = default;
    void apply_user_request(const std::string &v) { control(v); }
    const std::string &current_option() const { return state; }
    void publish_state(const std::string &v) { state = v; ++publishes; }
protected:
    virtual void control(const std::string &) = 0;
};
}
namespace lg_controller {
'''
source += real_classes
source += r'''
} // namespace lg_controller
} // namespace esphome
using esphome::lg_controller::LgSwitch;
using esphome::lg_controller::LgSelect;
int main() {
    LgSwitch sw;
    bool have_ca = false;
    sw.set_write_guard([&](bool) { return have_ca; });
    sw.apply_user_request(true);
    assert(!sw.state && sw.publishes == 0);  // no false HA success
    sw.publish_state(false);  // real CA always can publish
    assert(!sw.state && sw.publishes == 1);
    have_ca = true;
    sw.apply_user_request(true);
    assert(sw.state && sw.publishes == 2);

    LgSelect vane;
    bool vane_ready = false;
    vane.set_control_guard([&]() { return vane_ready; });
    vane.apply_user_request("6 (Up)");
    assert(vane.state.empty() && vane.publishes == 0);
    vane.publish_state("0 (Default)");  // genuine CA can still publish
    assert(vane.state == "0 (Default)" && vane.publishes == 1);
    vane_ready = true;
    vane.apply_user_request("6 (Up)");
    assert(vane.state == "6 (Up)" && vane.publishes == 2);

    // Floor Console write controls remain on last GENUINE observed state
    // until the component explicitly publishes a confirmed readback.
    LgSelect direction;
    direction.publish_state("Both");
    int requests = 0;
    std::string last_request;
    direction.set_request_handler([&](const std::string &v) {
        ++requests;
        last_request = v;
    });
    direction.apply_user_request("Bottom");
    assert(requests == 1 && last_request == "Bottom");
    assert(direction.state == "Both" && direction.publishes == 1);
    direction.publish_state("Bottom");  // strict mode's indoor confirmation
    assert(direction.state == "Bottom" && direction.publishes == 2);

    // Experimental optimistic discharge: the component's request handler
    // publishes the requested option at queue time; it does not need to
    // modify LgSelect behaviour for ANY other select entity.
    LgSelect fast_direction;
    fast_direction.publish_state("Both");
    int fast_requests = 0;
    fast_direction.set_request_handler([&](const std::string &v) {
        ++fast_requests;
        fast_direction.publish_state(v); // component's opt-in behaviour
    });
    fast_direction.apply_user_request("Bottom");
    assert(fast_requests == 1);
    assert(fast_direction.state == "Bottom" && fast_direction.publishes == 2);
    fast_direction.apply_user_request("Top");
    assert(fast_requests == 2 && fast_direction.state == "Top");

    LgSwitch comfort;
    comfort.publish_state(false);
    bool comfort_requested = false;
    comfort.set_request_handler([&](bool v) { comfort_requested = v; });
    comfort.apply_user_request(true);
    assert(comfort_requested && !comfort.state && comfort.publishes == 1);
    comfort.publish_state(true); // genuine indoor CC readback
    assert(comfort.state && comfort.publishes == 2);
}
'''
with tempfile.TemporaryDirectory() as td:
    src = Path(td) / "guard.cpp"
    exe = Path(td) / "guard"
    src.write_text(source)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-pedantic", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS: actual LgSwitch/LgSelect V2 guard implementations with mock ESPHome API")
