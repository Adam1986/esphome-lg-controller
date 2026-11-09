#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::lg_controller {

// V1.6 experimental Slave Type-B request policy. This helper deliberately
// does NOT know or alter installer settings. The application must validate
// the checksum on received AB/CB before calling observe(), and calculate
// the outgoing checksum after make_request().
class SlavePipeTempPoll {
public:
    static constexpr uint32_t POLL_INTERVAL_MS = 10u * 60u * 1000u;
    static constexpr uint32_t TEMPLATE_MAX_AGE_MS = 25u * 60u * 1000u;
    static constexpr uint32_t RESPONSE_WINDOW_MS = 90u * 1000u;
    static constexpr size_t FRAME_LEN = 13;

    // A genuine Master AB or genuine indoor CB is a valid current template.
    // A received Slave 2B is deliberately not accepted as authoritative.
    // Returns true if a CB arrived in the response window of our request.
    bool observe(const uint8_t *packet, uint32_t now_ms) {
        if (packet == nullptr || (packet[0] != 0xAB && packet[0] != 0xCB)) return false;
        bool response = packet[0] == 0xCB && awaiting_cb_ &&
                        static_cast<uint32_t>(now_ms - request_ms_) <= RESPONSE_WINDOW_MS;
        if (packet[0] == 0xCB) awaiting_cb_ = false;
        std::memcpy(template_, packet, FRAME_LEN);
        template_timestamp_ms_ = now_ms;
        has_template_ = true;
        return response;
    }

    bool has_template() const { return has_template_; }

    // True until an indoor CB response arrives or the response window expires.
    // A manual button must not start an overlapping query on the slow LG bus.
    bool awaiting_response(uint32_t now_ms) const {
        return awaiting_cb_ &&
               static_cast<uint32_t>(now_ms - request_ms_) <= RESPONSE_WINDOW_MS;
    }

    bool has_fresh_template(uint32_t now_ms) const {
        return has_template_ &&
               static_cast<uint32_t>(now_ms - template_timestamp_ms_) <= TEMPLATE_MAX_AGE_MS;
    }

    // Copy exact live installer fields; change only source and request flag.
    // Index 12 (checksum) must be recalculated by the caller.
    bool make_request(uint8_t *out, uint32_t now_ms) const {
        if (out == nullptr || !has_fresh_template(now_ms)) return false;
        std::memcpy(out, template_, FRAME_LEN);
        out[0] = 0x2B;
        out[1] |= 0x80;
        return true;
    }

    const char *template_source() const {
        return !has_template_ ? "none" : (template_[0] == 0xAB ? "Master AB" : "indoor CB");
    }

    void mark_sent(uint32_t now_ms) {
        awaiting_cb_ = true;
        request_ms_ = now_ms;
    }

private:
    uint8_t template_[FRAME_LEN] = {};
    uint32_t template_timestamp_ms_ = 0;
    bool has_template_ = false;
    bool awaiting_cb_ = false;
    uint32_t request_ms_ = 0;
};

}  // namespace esphome::lg_controller
