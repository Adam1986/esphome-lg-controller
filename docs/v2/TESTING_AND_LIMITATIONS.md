# V2.0.0 testing and limitations

## Confirmed as of 9 November 2026

- **Physical operation:** The maintainer reports that V2.0.0 was flashed to the installation's ESP32 controllers promptly after being built and ran successfully throughout the day. Actual on-device use included core power, target temperature, operating mode, and fan controls alongside physical PREMTB100 Masters. This is a user-reported deployment result, not independent certification of all features on all indoor models.
- **Standalone regression tests:** 19/19 C++17 test programs passed with the enhanced V2 implementation integrated into `esphome/components/lg_controller`. Python syntax and the mocked UI-guard test passed.
- **ESPHome configuration and compile:** `esphome config esphome/lg-example-standard.yaml` and `esphome compile esphome/lg-example-standard.yaml` passed using ESPHome **2026.10.2** in Codespaces. Flash usage was **864,960 / 1,835,008 bytes (47.1%)**. The build used locally generated test credentials and did not upload to any real device.
- **Floor Console compilation:** Successfully compiled with ESPHome 2026.10.2. Flash usage was 866,368 / 1,835,008 bytes (47.2%).
- **Component naming:** Active component is now `lg_controller`, matching the original upstream integration path. No separate development component directory is required.

## Pending Floor Console naming refactor

A post-V2.0.0 naming-only refactor changes internal helpers and the canonical
YAML keys to `floor_console_*`, retaining `floorstanding_*` as compatibility
aliases. The 19 existing C++ regression programs and additional Python alias
checks pass offline. A full ESPHome configuration validation and compilation
of both examples in Codespaces is required before merging this change.

## Known limits and areas for additional testing

- Both standard and floor console examples compile successfully. Model-specific floor console functionality still requires further hardware validation.
- **480-minute sleep reservation:** supported by the V2 Home Assistant input, but full set/readback/expiry remains to be physically confirmed. The Master countdown was observed at 420 minutes in earlier tests.
- Master-originated sleep edits, zero cancellation, mode changes, reboots and expiry should be tested across operating states and indoor models.
- Sleep-safe diagnostic polling, installer options, Auto Dry, advanced vane controls and Jet/Power Heating are model-dependent or experimental.
- The 100-baud setting is tested on this installation, not proven universal across all LG controller generations.
- Some energy, power, temperature and other advanced readings may remain unavailable or static on particular models.
- Check per-device pin assignments, chip revision and network exposure. The supplied base configs include an HTTP web server that should be reviewed for security.

## Development history

V2.0.0 represents the consolidated result of earlier local development, protocol investigation and hardware testing. The repository contains the enhanced implementation, regression tests and relevant technical findings. Earlier experimental builds and intermediate versions are not included.

## Recommended before a public tagged release

- [x] Pass 19 C++ standalone regression programs and Python/UI guard tests.
- [x] Compile the renamed standard Slave example with ESPHome 2026.10.2.
- [x] Record successful real-world operation of V2.0.0 across the installation.
- [x] Compile the floor console example with ESPHome 2026.10.2.
- [ ] Verify the merged public-name component on hardware if switching from an already-working internal development build (source logic is intended to match, but the renamed binary is newly compiled).
- [ ] Review credentials, secrets, log samples, original licences and attribution.
- [ ] Distinguish advanced features still under investigation from fully confirmed operation.

## V2.0.1 candidate Floor Console test plan

- Start with Garden Office only. Confirm the reboot log identifies V2.0.1.
- After reboot, before a `CD` has been received, request **Top** in HA.
  HA must keep its previous confirmed value; log `FLOOR TEMPLATE:` should
  show a guarded `28` read, followed by a genuine `CD` and then `FLOOR TX:
  page 5` for the selected value.
- Expect **two matching indoor `CD` reports** and `FLOOR TX: CONFIRMED`
  before HA displays Top as confirmed. Verify the physical discharge outlet.
- Repeat separately for **Both** and **Bottom**, allowing each transaction
  to finish. Observe the floor console's physical behaviour and wall Master.
- Test with no recent Master/indoor A8+C8 pair or active sleep. The guard
  must defer and time out with **no `2D` settings write** rather than
  transmitting fabricated fields. Floor Console sleep-active bootstrap is
  intentionally not enabled.
- Test a newer HA direction during an outstanding request: latest choice
  wins; older unconfirmed selections must not appear as a confirmed state.

The C++ tests cover pure frame/scheduling decisions, but cannot verify the
firmware task loop or the indoor unit's physical acceptance. Build both
example YAMLs with ESPHome 2026.10.2 and check actual on-device logs.
