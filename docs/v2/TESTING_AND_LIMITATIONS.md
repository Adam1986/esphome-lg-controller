# V2.0.0 testing and limitations

## Confirmed as of 9 November 2026

- **Physical operation:** The maintainer reports that V2.0.0 was flashed to the installation's ESP32 controllers promptly after being built and ran successfully throughout the day. Actual on-device use included core power, target temperature, operating mode, and fan controls alongside physical PREMTB100 Masters. This is a user-reported deployment result, not independent certification of all features on all indoor models.
- **Standalone regression tests:** 19/19 C++17 test programs passed with the enhanced V2 implementation integrated into `esphome/components/lg_controller`. Python syntax and the mocked UI-guard test passed.
- **ESPHome configuration and compile:** `esphome config esphome/lg-example-standard.yaml` and `esphome compile esphome/lg-example-standard.yaml` passed using ESPHome **2026.10.2** in Codespaces. Flash usage was **864,960 / 1,835,008 bytes (47.1%)**. The build used locally generated test credentials and did not upload to any real device.
- **Floor-standing compilation:** Successfully compiled with ESPHome 2026.10.2. Flash usage was 866,368 / 1,835,008 bytes (47.2%).
- **Component naming:** Active component is now `lg_controller`, matching the original upstream integration path. No separate development component directory is required.

## Known limits and areas for additional testing

- Both standard and floor-standing examples compile successfully. Model-specific floor-standing functionality still requires further hardware validation.
- **480-minute sleep reservation:** supported by the V2 Home Assistant input, but full set/readback/expiry remains to be physically confirmed. The Master countdown was observed at 420 minutes in earlier tests.
- Master-originated sleep edits, zero cancellation, mode changes, reboots and expiry should be tested across operating states and indoor models.
- Sleep-safe diagnostic polling, installer options, Auto Dry, advanced vane controls and Jet/Power Heating are model-dependent or experimental.
- The 100-baud setting is tested on this installation, not proven universal across all LG controller generations.
- Some energy, power, temperature and other advanced readings may remain unavailable or static on particular models.
- Check per-device pin assignments, chip revision and network exposure. The supplied base configs include an HTTP web server that should be reviewed for security.

## Development history

The V2.0.0 implementation underwent earlier internal development and testing before its successful hardware deployment and public ESPHome compilation. Previous development records remain available through Git history.

## Recommended before a public tagged release

- [x] Pass 19 C++ standalone regression programs and Python/UI guard tests.
- [x] Compile the renamed standard Slave example with ESPHome 2026.10.2.
- [x] Record successful real-world operation of V2.0.0 across the installation.
- [x] Compile the floor-standing example with ESPHome 2026.10.2.
- [ ] Verify the merged public-name component on hardware if switching from an already-working internal development build (source logic is intended to match, but the renamed binary is newly compiled).
- [ ] Review credentials, secrets, log samples, original licences and attribution.
- [ ] Distinguish advanced features still under investigation from fully confirmed operation.
