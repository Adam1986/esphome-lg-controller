#include <cassert>
#include <cstdio>
#include <cstring>
#include "slave_esp_calibration.h"
using esphome::lg_controller::SlaveEspCalibration;
int main() {
    uint8_t ca[13]={0xCA,0x06,0,0,0,0,0,0x05,0,0x92,0xF1,0x21,0};
    for (int idx=0;idx<4;idx++) {
        uint8_t out[13]={};
        assert(SlaveEspCalibration::make_write(out,ca,idx,125));
        for (int i=0;i<12;i++) {
            if (i==0) assert(out[i]==0x2A);
            else if (i==2+idx) assert(out[i]==125);
            else assert(out[i]==ca[i]);
        }
    }
    uint8_t out[13]={};
    assert(!SlaveEspCalibration::make_write(out,ca,4,50));
    assert(!SlaveEspCalibration::make_write(out,ca,0,256));
    uint8_t bad[13]={0xAA};
    assert(!SlaveEspCalibration::make_write(out,bad,0,50));
    puts("PASS: ESP installer calibration one-field-only writes");
}
