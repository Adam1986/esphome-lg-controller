# LG Controller V2 — PREMTB100 Master / ESP32 Slave improvements

> **Status: hardware-tested community fork, with experimental model-specific functions.** The V2.0.0 baseline was deployed on 9 November 2026, and both example configurations compiled with ESPHome **2026.10.2**. V2.0.4 Floor Console fast discharge has also been observed on real hardware, including delayed indoor `CD` confirmation of Bottom. The newest merged changes have not been certified for every unit or operating state. This is **not an official upstream release**.

## Background and credits

This repository is a direct fork of [JanM321/esphome-lg-controller](https://github.com/JanM321/esphome-lg-controller). The V2 work was built with reference to functionality and enhancements in [balazsborbely/esphome-lg-controller](https://github.com/balazsborbely/esphome-lg-controller); thanks to both authors and other contributors. Existing source licensing and attribution remain in place.

Most of the additional protocol analysis and development was **AI-assisted**. The maintainer supplied real equipment, protocol captures, test scenarios, iterative feedback, and on-device validation. The additional reverse engineering includes publicly available GPL transport-driver information, separate controller firmware research, and captured traffic. No extracted LG firmware images are included here.

## Purpose

Improve **ESP32 Slave** control while retaining physical LG **PREMTB100 Master** controllers and their room temperature sensors. Compared with the initially unreliable configuration, testing showed substantially improved retention of power, operating-mode, fan-speed, and set-temperature commands.

## Main changes and findings

- **100-baud communication:** analysis of an LG TTL driver suggested a bit period of 20 × 500 µs (100 bps), versus 104 bps described in earlier community documentation. This has worked well in the tested installation but should not be assumed suitable for every generation of LG controller.
- **Slave state/command handling:** preserve genuine Master settings, improve command construction, startup synchronisation, pending-change arbitration, retries, and readback confirmation from the LG system.
- **Sleep reservations:** track remaining time reported by the Master (`A8`) rather than relying only on an independent ESP32 countdown. Improve set/cancel synchronisation and avoid competing local expiry shutdowns. Cancellation and model-specific edge cases still require continued testing.
- **8-hour support:** many tested wall-controller models expose up to 480 minutes. V2's Home Assistant input supports 0–480 minutes, but the full 480-minute set/readback/expiry flow has not yet been proven end to end.
- **Diagnostics/features:** extended discovery, pipe-temperature handling, installer-related diagnostics and floor console optional controls; actual support depends on indoor unit model.

> **LG ARNU07GQAA4 Floor Console — experimental:** The additional Floor Console controls are still undergoing physical testing. The Top / Bottom / Both discharge-direction path has produced genuine indoor-unit confirmations in limited tests, but this does not establish reliable behaviour across all functions or conditions. Comfort Cooling, Comfort Cooling Step and other console-specific behaviour may not work as expected until further verification. Optional fast discharge displays the **requested** setting after a UART echo and verifies it later, so the selector can temporarily differ from the actual indoor-unit state. See [testing and limitations](docs/v2/TESTING_AND_LIMITATIONS.md).

A valid transmit or UART echo should **not** be treated as proof that the indoor unit accepted a command. Readback from the physical controller and indoor unit remains important.

## Repository layout

- [`esphome/components/lg_controller/`](esphome/components/lg_controller/) — enhanced public ESPHome component (same name as original upstream).
- [`esphome/base-lg-ac.yaml`](esphome/base-lg-ac.yaml) — shared standard-unit base configuration.
- [`esphome/base-lg-ac-floor-console.yaml`](esphome/base-lg-ac-floor-console.yaml) — optional floor console base.
- [`esphome/lg-example-standard.yaml`](esphome/lg-example-standard.yaml) and [`esphome/lg-example-floor-console.yaml`](esphome/lg-example-floor-console.yaml) — sample configurations (require your own local secrets and sensor entities).
- [`esphome/tests/`](esphome/tests/) and [`scripts/run-v2-offline-tests.sh`](scripts/run-v2-offline-tests.sh) — regression tests.
- [`docs/v2/`](docs/v2/) — research and validation notes.

## Testing and use

In a checkout of this repository, create **private** `esphome/secrets.yaml` values for the example and ensure the file is Git-ignored. For the standard Slave example:

```bash
bash scripts/run-v2-offline-tests.sh
esphome config esphome/lg-example-standard.yaml
esphome compile esphome/lg-example-standard.yaml
```

Both standard and floor console examples compiled successfully with ESPHome 2026.10.2 on 9 November 2026. Standalone tests are not a replacement for testing actual hardware. Model-specific floor console functions still require additional hardware validation.

Review example hardware settings before flashing: GPIOs, ESP32 chip revision, UART timing, indoor-unit capabilities and the HTTP `web_server` configuration may not suit all installations. Keep the physical wall controller as Master if relying on its built-in temperature sensor. **Never commit credentials, local secrets, personal logs or extracted controller firmware.**

## Documentation

- [Protocol notes](docs/v2/PROTOCOL_NOTES.md)
- [Sleep reservation improvements](docs/v2/SLEEP_RESERVATIONS.md)
- [Testing and limitations](docs/v2/TESTING_AND_LIMITATIONS.md)
- [Change history](docs/v2/CHANGELOG_V2.md)
- [Floor Console fast discharge (experimental)](docs/v2/FLOOR_FAST_DISCHARGE_V204.md)
- [Historical Floor Console readback research](docs/v2/research/FLOOR_CONSOLE_INVESTIGATION.md)

## Contributions

Reports from other LG controller and indoor-unit combinations are welcome. When reporting results, please distinguish a successfully transmitted packet from genuine Master/indoor-unit confirmation, and include relevant configuration, anonymised logs and component version when possible.

## ESPHome framework and device diagnostics

In addition to the LG communication improvements, the V2.0.0
shared YAML configurations incorporate several standard ESPHome
features for hardware configuration, monitoring and management.

These features are included in both the standard and Floor Console
configurations, regardless of whether the ESP32 operates as Master
or Slave.

### ESP-IDF framework configuration

The supplied configurations use the ESP-IDF framework:

```yaml
framework:
  type: esp-idf
  advanced:
    minimum_chip_revision: "3.1"
    sram1_as_iram: true
    flash_chip: generic
```

- **ESP-IDF:** The framework used for the tested ESP32 builds.
- **Minimum chip revision 3.1:** Targets compatible original ESP32
  silicon revisions, rather than older revisions.
- **SRAM1 as IRAM:** Makes additional memory available for
  instruction storage on supported ESP32 hardware.
- **Generic flash driver:** Selects the generic flash-chip
  implementation.

**Hardware compatibility:** These settings reflect the ESP32
hardware used during development. They should not be assumed
appropriate for older ESP32 revisions or other ESP32 families,
including ESP32-S3. In particular, the SRAM1/IRAM option has
bootloader requirements.

Users adapting these examples should review the ESP32 board,
variant and advanced framework settings before flashing.

### ESP32 Reboot

Both configurations expose an **ESP32 Reboot** button in
Home Assistant under the diagnostic entities.

It allows the ESP32 to be restarted remotely without physically
disconnecting its power supply.

This restarts the ESP32, not the LG indoor unit.

### Wi-Fi Quality

A diagnostic sensor reports an estimated Wi-Fi signal strength
from 0% to 100%, calculated from the measured RSSI.

The supplied configuration maps:

- -100 dBm or weaker to 0%
- -75 dBm to approximately 50%
- -50 dBm or stronger to 100%

The value is updated every five minutes.

This is a convenient signal-strength indicator rather than
a measurement of network throughput or connection reliability.

### Wi-Fi SSID

The **Wi-Fi SSID** diagnostic text sensor displays the name
of the wireless network to which the ESP32 is connected.

This can be useful when diagnosing connectivity or checking
which network an individual controller is using.

### Last Boot

The **Last Boot** diagnostic uses ESPHome's uptime timestamp
sensor to report the approximate time the ESP32 last started.

This can help identify unexpected restarts during troubleshooting.

### Master and Slave operation

All these features operate independently of the LG controller's
Master/Slave setting.

They are implemented through standard ESPHome configuration
and do not require additional LG protocol messages.

The supplied configurations are validated with ESPHome 2026.10.2.
Hardware-specific framework settings should be adapted when
using different ESP32 boards.

## Floor Console naming compatibility

The implementation uses `floor_console_*` identifiers and helper headers
(e.g. `floor_console_flow.h`, `floor_console_wire.h`, and
`floor_console_decode.h`) to match LG's **Floor Console** terminology.

New examples use `floor_console_*` YAML options. Configurations using the
previous `floorstanding_*` options remain compatible: the component converts
those legacy options to the new names during YAML validation. Do not supply
both spellings for the same option. The existing Home Assistant entity display
names are intentionally preserved, avoiding unplanned changes to entity IDs.

This is a naming refactor; LG packet formats, control behaviour, and vane
wire mappings have not been intentionally changed.

## Floor Console discharge control and readback

The merged Floor Console component can send ordinary genuine-template-based
`2D` commands for **Top / Bottom / Both**. Unlike standard temperature, mode
and fan control (`28` / `C8`), the indoor unit may not publish page-5 `CD`
readback promptly unless a full settings refresh is requested.

The normal Floor Console path retains its guarded transmission and genuine
indoor-unit confirmation rules. **Optional**
`floor_console_fast_discharge: true` changes the user-facing behaviour for
discharge direction only: after the transmitted command echoes, Home Assistant
shows the *requested but unverified* choice. A coalesced full settings refresh
checks for two consistent genuine indoor `CD` messages after 45 seconds of
quiet and when the bus is available. The command-status entity distinguishes
`SENT (UNVERIFIED)` from a later `CONFIRMED` result.

On 10 November 2026, the Garden Office `ARNU07GQAA4` produced a genuine indoor
`CD` confirmation of Bottom after the delayed refresh. This is **not** proof
of instant physical outlet movement or of reliable behaviour across every
mode, power state, or rapid sequence. Comfort Cooling and Comfort Cooling Step
remain under testing. See the [fast-discharge guide](docs/v2/FLOOR_FAST_DISCHARGE_V204.md)
and [testing limitations](docs/v2/TESTING_AND_LIMITATIONS.md).

Earlier attempts to observe a spontaneous `CD` and to query it using a
speculative `2D.80` frame did not produce timely readback. Both options are
**off by default** and should remain disabled; `2D.80` has **not** been
verified as a read-only operation. The [research archive](docs/v2/research/FLOOR_CONSOLE_INVESTIGATION.md)
records what was learned, without obsolete installation instructions.
