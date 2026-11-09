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
}
'''
with tempfile.TemporaryDirectory() as td:
    src = Path(td) / "guard.cpp"
    exe = Path(td) / "guard"
    src.write_text(source)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-pedantic", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS: actual LgSwitch/LgSelect V2 guard implementations with mock ESPHome API")
