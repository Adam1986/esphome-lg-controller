#pragma once
#include <cstdint>
#include <cstdio>

// V1.8.5-V1.8.7: read-only decoding of the Central Control Address (Type-A byte 1).
// Feed only checksum-verified 0xCA packets from the genuine indoor unit.
// Two consecutive CA observations must agree before the address is accepted.
// No UART transmission or modification of installer settings occurs here.
class SlaveCentralControlAddress {
public:
    bool observe(uint8_t observed_address) {
        if (candidate_count_ == 0 || candidate_ != observed_address) {
            candidate_ = observed_address;
            candidate_count_ = 1;
        } else if (candidate_count_ < 2) {
            ++candidate_count_;
        }
        if (candidate_count_ < 2) return false;
        if (has_confirmed_ && confirmed_address_ == observed_address) return false;
        confirmed_address_ = observed_address;
        has_confirmed_ = true;
        return true; // first confirmed value or confirmed new address
    }

    bool has_confirmed_address() const { return has_confirmed_; }
    uint8_t address() const { return confirmed_address_; }
    // Format one read-only diagnostic as HEX / decimal, e.g. "0A / 10".
    // Longest possible is "FF / 255" (8 chars plus NUL).
    static void format_display(uint8_t address, char (&output)[9]) {
        std::snprintf(output, sizeof(output), "%02X / %u",
                      static_cast<unsigned>(address), static_cast<unsigned>(address));
    }
    static uint8_t decimal_value(uint8_t address) { return address; }

    static void format_hex(uint8_t address, char (&output)[3]) {
        static constexpr char hex[] = "0123456789ABCDEF";
        output[0] = hex[(address >> 4) & 0x0F];
        output[1] = hex[address & 0x0F];
        output[2] = '\0';
    }

private:
    uint8_t candidate_ = 0;
    uint8_t candidate_count_ = 0;
    uint8_t confirmed_address_ = 0;
    bool has_confirmed_ = false;
};
