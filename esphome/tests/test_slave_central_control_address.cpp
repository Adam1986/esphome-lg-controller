#include <cassert>
#include <cstdio>
#include <cstring>
#include "../components/lg_controller/slave_central_control_address.h"

int main() {
    SlaveCentralControlAddress s;
    char text[3]{};
    char combined[9]{};
    assert(!s.has_confirmed_address());
    assert(!s.observe(0x03));
    assert(!s.has_confirmed_address());
    assert(s.observe(0x03));
    assert(s.has_confirmed_address() && s.address() == 0x03);
    SlaveCentralControlAddress::format_hex(s.address(), text);
    assert(std::strcmp(text, "03") == 0);
    SlaveCentralControlAddress::format_display(s.address(), combined);
    assert(std::strcmp(combined, "03 / 3") == 0);
    assert(SlaveCentralControlAddress::decimal_value(s.address()) == 3);
    assert(!s.observe(0x03));
    assert(!s.observe(0x0A));
    assert(s.address() == 0x03); // single changed CA must not overwrite
    assert(!s.observe(0x03));  // breaks candidate pair, still old value
    assert(!s.observe(0x0A));
    assert(s.observe(0x0A));
    assert(s.address() == 0x0A);
    SlaveCentralControlAddress::format_hex(s.address(), text);
    assert(std::strcmp(text, "0A") == 0);
    SlaveCentralControlAddress::format_display(s.address(), combined);
    assert(std::strcmp(combined, "0A / 10") == 0);
    assert(SlaveCentralControlAddress::decimal_value(s.address()) == 10);
    assert(!s.observe(0xFF));
    assert(s.observe(0xFF));
    SlaveCentralControlAddress::format_hex(s.address(), text);
    assert(std::strcmp(text, "FF") == 0);
    SlaveCentralControlAddress::format_display(s.address(), combined);
    assert(std::strcmp(combined, "FF / 255") == 0);
    assert(SlaveCentralControlAddress::decimal_value(s.address()) == 255);
    assert(!s.observe(0x00));
    assert(s.observe(0x00));
    SlaveCentralControlAddress::format_hex(s.address(), text);
    assert(std::strcmp(text, "00") == 0);
    SlaveCentralControlAddress::format_display(s.address(), combined);
    assert(std::strcmp(combined, "00 / 0") == 0);
    assert(SlaveCentralControlAddress::decimal_value(s.address()) == 0);
    assert(!s.observe(0x00));
    puts("PASS: Slave Central Control Address: two-frame confirmation, change detection, single combined hex / decimal diagnostic");
    return 0;
}
