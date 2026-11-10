#include <cassert>
#include <cstdint>
#include "../components/lg_controller/floor_console_flow.h"
#include "../components/lg_controller/slave_pipe_temp_discovery.h"
using namespace lg_floor_console;
using esphome::lg_controller::SlavePipeTempDiscovery;

int main() {
    // Opt-in passive CD window is limited to discharge and Slave mode.
    assert(readback_settle_ms(true, Page::Discharge, true, 2500, 12000) == 12000);
    assert(readback_settle_ms(true, Page::Discharge, false, 2500, 12000) == 2500);
    assert(readback_settle_ms(false, Page::Discharge, true, 2500, 12000) == 2500);
    assert(readback_settle_ms(true, Page::Comfort, true, 2500, 12000) == 2500);
    assert(readback_settle_ms(true, Page::Step, true, 2500, 12000) == 2500);
    // No full-settings refresh before the observation window closes.
    assert(!readback_due(true, 1, 0, true, false, 11999, 12000));
    assert(readback_due(true, 1, 0, true, false, 12000, 12000));
    // An observed/confirmed CD completes a transaction outside this helper;
    // a superseding HA request must cancel its queued readback in the host.

    assert(!readback_due(false,1,0,true,false,3000,2500)); // only Slave uses guarded 28
    assert(!readback_due(true,0,0,true,false,3000,2500));
    assert(!readback_due(true,1,0,false,false,3000,2500));
    assert(!readback_due(true,1,0,true,false,1000,2500));
    assert(readback_due(true,1,0,true,false,2500,2500));
    assert(!readback_due(true,1,0,true,true,2500,2500));
    assert(!readback_due(true,1,1,true,false,3000,2500));
    assert(readback_due(true,2,1,true,false,3000,2500));
    assert(!retry_due(0,false,false,65000,65000));
    assert(!retry_due(1,false,false,10000,65000));
    assert(!retry_due(1,true,false,66000,65000));
    assert(!retry_due(1,false,true,66000,65000));
    assert(retry_due(1,false,false,65000,65000));
    assert(!retry_due(2,false,false,200000,65000)); // bounded writes
    assert(template_refresh_due(0, 0, 75000, 2)); // first read, no template
    assert(!template_refresh_due(1, 5000, 75000, 2)); // not a write/retry yet
    assert(template_refresh_due(1, 75000, 75000, 2)); // one bounded retry
    assert(!template_refresh_due(2, 100000, 75000, 2)); // no unbounded reads
    assert(frame_is_newer(10,9));
    assert(!frame_is_newer(9,10));
    assert(frame_is_newer(10,0xFFFFFFF0u)); // 32-bit millis rollover

    // Readback must be built from genuine matching observed A8 + C8, not a
    // fabricated Type-0 status or a stale/mismatched pair.
    uint8_t master[13]={0xA8,0x90,0,0,0x40,0x01,0x56,0x16,0,0,0x04,0,0};
    uint8_t indoor[13]={0xC8,0x90,0,0,0x40,0x01,0x56,0x16,0,0,0,0,0};
    uint8_t out[13]={};
    assert(SlavePipeTempDiscovery::make_request(out,master,indoor,1000,1000));
    assert(out[0]==0x28 && out[8]==0x40 && (out[1]&1)==0);
    indoor[6]=0x50; // conflicting setpoint: no readback
    assert(!SlavePipeTempDiscovery::make_request(out,master,indoor,1000,1000));
    indoor[6]=0x56;
    assert(!SlavePipeTempDiscovery::make_request(out,master,indoor,95000,1000));
    return 0;
}
