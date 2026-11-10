#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "../components/lg_controller/slave_pipe_temp_poll.h"
using esphome::lg_controller::SlavePipeTempPoll;

static void test_master_template_preserves_settings() {
    SlavePipeTempPoll policy;
    uint8_t input[13] = {0xAB,0x00,0x20,0x76,0x76,0x00,0x80,0x02,0x10,0x41,0x40,0x00,0x9F};
    uint8_t request[13] = {};
    assert(!policy.has_template());
    assert(!policy.make_request(request, 1000));
    assert(!policy.observe(input, 1000));
    assert(policy.has_template());
    assert(policy.has_fresh_template(1000));
    assert(std::strcmp(policy.template_source(), "Master AB") == 0);
    assert(policy.make_request(request, 1000));
    assert(request[0] == 0x2B);
    assert(request[1] == 0x80);
    assert(std::memcmp(request+2,input+2,10)==0);
    assert(input[0]==0xAB && input[1]==0);  // never alter source
}

static void test_unit_template_high_bit_and_settings_preserved() {
    SlavePipeTempPoll policy;
    uint8_t input[13] = {0xCB,0x80,0x20,0x77,0x76,0x00,0x80,0x02,0x10,0x41,0x40,0x00,0x3E};
    uint8_t output[13] = {};
    assert(!policy.observe(input,25000));
    assert(policy.make_request(output,25001));
    assert(output[0]==0x2B && output[1]==0x80);
    assert(std::memcmp(output+2,input+2,10)==0);
    assert(std::strcmp(policy.template_source(),"indoor CB")==0);
}

static void test_stale_template_fails_closed() {
    SlavePipeTempPoll policy;
    uint8_t sample[13] = {0xAB};
    uint8_t output[13] = {};
    policy.observe(sample,1000);
    assert(policy.has_fresh_template(1000+SlavePipeTempPoll::TEMPLATE_MAX_AGE_MS));
    assert(!policy.make_request(output,1001+SlavePipeTempPoll::TEMPLATE_MAX_AGE_MS));
    assert(!policy.has_fresh_template(1002+SlavePipeTempPoll::TEMPLATE_MAX_AGE_MS));
}

static void test_ignore_slave_and_status_packets() {
    SlavePipeTempPoll policy;
    uint8_t slave[13] = {0x2B,0x80};
    uint8_t status[13] = {0xC8,0x72};
    assert(!policy.observe(slave,1000));
    assert(!policy.observe(status,1000));
    assert(!policy.has_template());
}

static void test_response_and_unsolicited_cb() {
    SlavePipeTempPoll policy;
    uint8_t sample[13] = {0xAB};
    uint8_t cb[13] = {0xCB};
    assert(!policy.observe(sample,1000));
    assert(!policy.observe(cb,1001)); // Unsolicited
    policy.mark_sent(2000);
    assert(policy.observe(cb,2000+20000)); // CB in 90s window
    assert(!policy.observe(cb,2000+30000)); // Already consumed
    policy.mark_sent(5000);
    assert(!policy.observe(cb,5000+SlavePipeTempPoll::RESPONSE_WINDOW_MS+1));
}

static void test_refresh_request_inflight() {
    SlavePipeTempPoll policy;
    uint8_t cb[13] = {0xCB};
    assert(!policy.awaiting_response(1000));
    policy.observe(cb, 1000);
    policy.mark_sent(1200);
    assert(policy.awaiting_response(1200));
    assert(policy.awaiting_response(1200 + SlavePipeTempPoll::RESPONSE_WINDOW_MS));
    assert(!policy.awaiting_response(1201 + SlavePipeTempPoll::RESPONSE_WINDOW_MS));
    policy.mark_sent(2000);
    assert(policy.observe(cb, 2100));
    assert(!policy.awaiting_response(2101));
}

static void test_rollover_millis() {
    SlavePipeTempPoll policy;
    uint8_t cb[13] = {0xCB};
    const uint32_t near_wrap = 0xFFFFFF00u;
    policy.observe(cb,near_wrap);
    uint8_t output[13];
    assert(policy.make_request(output,200u));
    policy.mark_sent(near_wrap);
    assert(policy.observe(cb,500u));
}

int main(){
    test_master_template_preserves_settings();
    test_unit_template_high_bit_and_settings_preserved();
    test_stale_template_fails_closed();
    test_ignore_slave_and_status_packets();
    test_response_and_unsolicited_cb();
    test_rollover_millis();
    test_refresh_request_inflight();
    std::puts("7 slave pipe poll policy regression tests passed");
}
