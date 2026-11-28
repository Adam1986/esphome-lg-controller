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
- **Heating / idle operating state:** genuine indoor status is authoritative in Slave mode, with automatically learned, persistent CF fan feedback refining HEATING/IDLE on supported models. Unsupported units retain the ordinary status routine. See [heating / idle operating-state detection](#heating--idle-operating-state-detection-november-2026).
- **Diagnostics/features:** extended discovery, pipe-temperature handling, installer-related diagnostics and floor console optional controls; actual support depends on indoor unit model.

> **LG ARNU07GQAA4 Floor Console — experimental:** The additional Floor Console controls are still undergoing physical testing. The Top / Bottom / Both discharge-direction path has produced genuine indoor-unit confirmations in limited tests, but this does not establish reliable behaviour across all functions or conditions. Comfort Cooling, Comfort Cooling Step and other console-specific behaviour may not work as expected until further verification. Optional fast discharge displays the **requested** setting after a UART echo and verifies it later, so the selector can temporarily differ from the actual indoor-unit state. See [testing and limitations](docs/v2/TESTING_AND_LIMITATIONS.md).

A valid transmit or UART echo should **not** be treated as proof that the indoor unit accepted a command. Readback from the physical controller and indoor unit remains important.

## Heating / idle operating-state detection (November 2026)

### Investigation and findings

Home Assistant distinguishes the selected climate mode (`HEAT`) from the
current climate action (`HEATING` or `IDLE`). In the tested ten-zone
installation with PREMTB100 Masters and ESP32 Slaves, some ducted and Floor
Console indoor units continued to report `HEATING` after satisfying their
temperature demand and stopping their indoor fan. The shared outdoor unit
could remain running to serve another zone. The five-unit ESP32 Master
installation used for comparison did not exhibit the same problem in the
observed heating cycles.

The investigation compared genuine Master `A8` and indoor `C8` status,
extended indoor `CF` messages, installer fan settings and physical equipment
behaviour. Changing UART timing from 100 to 104 baud during testing did not
resolve the discrepancy. The outdoor-running flag alone was insufficient
to distinguish local heating demand from shared outdoor activity on some
units. The upper two bits of C8 byte 7 also differed between the installations:
the heating-direction flag was commonly populated in the Master installation
but absent in the tested Slave installation. Those bits are not established
as a per-indoor-unit Thermo ON/OFF indication.

A useful additional observation was that **CF byte 11 (zero-based indexing)**
tracked actual indoor fan activity on several ducted units and the tested
Floor Console. Nonzero values broadly corresponded to the installer fan-speed
settings; zero was observed when the fan stopped. The three tested wall-mounted
units instead returned zero in this field even during confirmed heating, with
CF bytes 9 and 10 also unpopulated. A zero reading alone therefore cannot
establish that a fan has stopped.

### Implemented approach

- **Authoritative operating status:** in Slave mode only genuine indoor
  `C8` status updates operating telemetry and climate action. Master `A8`
  messages remain important for settings, temperature and synchronisation,
  but cannot overwrite indoor operating feedback. Operating status is
  evaluated independently of pending command and settings-acceptance guards.
  Slave action is not set optimistically from a Home Assistant mode request.
- **Outdoor status debounce:** retain the eight-second suppression of brief
  outdoor ON/OFF chatter, measuring from an accepted state transition rather
  than repeatedly restarting the timer on rejected readings.
- **Automatic fan capability detection:** learn support from a genuine
  nonzero indoor `CF` fan reading. Only refine an existing C8-derived
  `HEATING` action in explicit Slave HEAT mode. A valid, fresh zero can
  refine it to `IDLE`; a nonzero value retains `HEATING`.
- **Unsupported / ambiguous feedback:** an all-zero CF byte 9/10/11 group
  cannot establish a stopped fan, even if capability was previously learned.
  Unsupported units retain the ordinary C8 operating-state routine. This
  preserves the tested wall-mounted units' behaviour without model lists.
- **Persistent capability:** save the learned capability in a separate
  ESPHome preference on first successful detection, without changing the
  existing C9 capability storage. Restore only the learned marker after a
  reboot, never the actual fan reading. The first installation of this change
  still requires a nonzero reading before support can be saved. A failed
  preference write leaves detection available for the current boot.
- **Freshness and priority:** require new fan evidence after transitions into
  or out of a C8 heating candidate; expire fan inference after three minutes
  without a fresh valid sample. OFF, defrost, preheat and the ordinary
  non-heating actions remain authoritative. Master mode does not use the
  fan-based action refinement.

The refinement is passive: no additional LG polling, UART commands, LGAP
integration or base-YAML changes are required. The same implementation is
used in the standard Slave and experimental V2.0.6 Floor Console branches.
It does not change the Floor Console discharge-control or readback scheduling.

### Automatic diagnostic sensor

**Actual Indoor Fan Speed (Raw)** (`actual_fan_speed_raw`) is automatically
registered as a Home Assistant diagnostic sensor. Existing explicit YAML
definitions of this sensor remain supported. It exposes the raw indoor CF
byte, including zero before capability is established, and becomes unknown
when no new CF reading has been received for three minutes. The sensor is
independent of the action-inference gate.

| Raw diagnostic | Interpretation |
| --- | --- |
| Nonzero (e.g. 78, 88) | Observed fan feedback; raw protocol value, not confirmed RPM or percent |
| Zero, learned support and populated CF temperature fields | Usable stopped-fan evidence in the supported Slave HEAT context |
| Zero, support not learned or CF temperature fields empty | Ambiguous; retain C8-derived action |
| Unknown / stale | No current CF diagnostic reading; do not rely on fan inference |

Useful log messages include `SLAVE HEAT: detected indoor CF fan feedback`,
`SLAVE HEAT: saved learned CF fan feedback support`, and
`SLAVE HEAT: restored learned CF fan feedback support; awaiting fresh sample`.
An action transition driven by the CF refinement is logged with
`SLAVE HEAT: IDLE (CF fan feedback / C8 fallback)` or its HEATING equivalent.

### Hardware observations and limitations

On 28 November 2026, a post-update capture from approximately 11:26 to 13:35
contained 2,222 checksum-valid C8 and 2,077 checksum-valid CF messages across
ten indoor units. All 646 CF frames from the three wall-mounted units had
zero fan and temperature fields; no CF-driven action transitions were logged
for those units. One ducted unit repeatedly changed between HEATING and IDLE
following nonzero and zero fan feedback. Another wall-mounted unit completed
multiple ordinary C8-based heating cycles despite its constant zero raw fan
reading. Two ducted units logged successful capability storage earlier in the
capture and subsequently entered fan-derived IDLE with zero-only feedback
after the update, consistent with persisted capability being used. Some units
had not yet produced a nonzero reading with persistence enabled and therefore
still needed an active heating cycle to learn support.

The observations support the approach on the tested equipment, but **CF fan
feedback is an empirical proxy for heating activity, not a decoded Thermo
ON/OFF flag**. A unit with continuous fan operation during thermo-off may
still report HEATING; other models may encode these fields differently. A
long preheat period can legitimately keep the climate action IDLE even when
the selected mode is HEAT. Unsupported units may still be limited by the
original C8 flags. The three-minute timeout is a conservative implementation
choice, not a documented LG protocol guarantee. Full validation across other
models, cooling, all operating modes and repeated power cycles remains
necessary.

The regression cases in
[`test_lg_operating_status.cpp`](esphome/tests/test_lg_operating_status.cpp)
cover source selection, outdoor debounce, learned capability restoration,
zero-only units, empty CF telemetry, freshness, mode transitions and timer
wraparound. Offline tests do not replace live equipment validation.

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
quiet (default), or after a longer opt-in quiet interval using
`floor_console_fast_verify_quiet_seconds: 120` for experiments on the Garden
Office. New user controls reset the quiet clock; an absolute deadline still
bounds verification. The command-status entity distinguishes
`SENT (UNVERIFIED)` from a later `CONFIRMED` result.

**V2.0.6 experiment (opt-in):** On a Slave Floor Console with fast discharge
already enabled, `floor_console_optimistic_discharge: true` makes the existing
Top/Both/Bottom dropdown show a new choice **as soon as the user requests it**,
including while the command is safely queued. It is a requested value, **not**
an indoor acknowledgement. A genuinely fresh matching indoor `CD` after the
scheduled readback confirms it on the first sample; rejection still requires
two consistent mismatching samples. Existing optimistic display is retained
on an unverified timeout, with a diagnostic log; a subsequent genuine indoor
`CD` may resynchronise it. Physical Master changes take priority. This does
not speed up the actual LG bus or alter normal climate/sleep controls. Default
is `false`, and it requires Slave, fast discharge and Floor Console features.

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
