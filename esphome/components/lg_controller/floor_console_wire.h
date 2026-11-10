#pragma once
#include <cstdint>
#include <cstring>

// Wire protocol field mapping recovered from PREMTB100 V2.12.3a (AC build).
// The source frame must be real and checksum-valid. For a Slave, prefer a
// genuine Master page; if none is available, a genuine indoor report of the
// SAME page may supply the shared page payload. No zero-filled installer frames.
// Only change the targeted field, source header, and checksum.
namespace lg_floor_console {
enum class Page : uint8_t { Comfort = 4, Discharge = 5, Step = 6 };

inline uint8_t checksum(const uint8_t *bytes) {
    uint8_t sum = 0;
    for (unsigned i = 0; i != 12; ++i) sum = uint8_t(sum + bytes[i]);
    return sum ^ 0x55;
}
inline bool recognized_frame(const uint8_t *f, Page page) {
    return f != nullptr && (f[0] & 7u) == static_cast<uint8_t>(page) &&
           ((f[0] & 0xF8u) == 0xA8u || (f[0] & 0xF8u) == 0xC8u) &&
           (page != Page::Step || f[1] == 0x10) && checksum(f) == f[12];
}
// EXPERIMENTAL selective Type-D read candidate. It is NOT a proven LG read
// opcode. Clone the just-transmitted 2D *write* so all 12 payload bytes,
// including the requested discharge direction, are identical. If an indoor
// unit treats 2D.80 as a write, it won't restore the previously observed
// direction. Byte 1 bit 7 is speculative and may have other meanings.
inline bool build_selective_cd_probe(uint8_t out[13], const uint8_t last_sent[13]) {
    if (out == nullptr || last_sent == nullptr || last_sent[0] != 0x2Du ||
        checksum(last_sent) != last_sent[12]) return false;
    std::memcpy(out, last_sent, 13);
    out[1] |= 0x80u;
    out[12] = checksum(out);
    return true;
}
inline uint8_t value(const uint8_t *f, Page page) {
    switch (page) {
      case Page::Comfort:   return (f[11] & 0x04u) != 0;
      case Page::Discharge: return f[8] & 0x03u;
      case Page::Step:      return f[5] & 0x0Fu;
    }
    return 0xFF;
}
inline bool valid_value(Page page, uint8_t v) {
    return page == Page::Comfort ? v <= 1 : v <= 2;
}
inline bool build(uint8_t out[13], const uint8_t source[13], Page page,
                  bool slave, uint8_t requested) {
    if (!valid_value(page, requested) || !recognized_frame(source, page)) return false;
    // Master controls must use indoor-unit frames. A Slave may use either
    // genuine Master or indoor-unit frames; never copy a Slave/unknown source.
    const uint8_t source_role = source[0] & 0xF8u;
    if ((slave && source_role != 0xA8u && source_role != 0xC8u) ||
        (!slave && source_role != 0xC8u)) return false;
    std::memcpy(out, source, 13);
    out[0] = uint8_t((slave ? 0x28u : 0xA8u) | static_cast<uint8_t>(page));
    switch (page) {
      case Page::Comfort:
        out[11] = uint8_t((out[11] & ~0x04u) | (requested ? 0x04u : 0x00u));
        // Type-4 byte 7 bit 7 is the documented "settings changed" flag.
        out[7] |= 0x80u;
        break;
      case Page::Discharge:
        out[8] = uint8_t((out[8] & ~0x03u) | requested);
        break;
      case Page::Step:
        out[1] = 0x10;
        out[5] = uint8_t((out[5] & 0xF0u) | requested);
        break;
    }
    out[12] = checksum(out);
    return true;
}
}
