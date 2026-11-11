# V2 testing status and limitations

**Last updated: 11 November 2026.** This is a hardware-tested community fork,
not an official LG or upstream release. Passing offline tests, an ESPHome
compile and a successful UART echo each establish different things; none
alone proves every advanced setting was accepted by an indoor unit.

## Established baseline

- **Physical operation (9 November 2026):** The maintainer reported
  successful V2.0.0 operation across an LG installation using physical
  PREMTB100 Masters and ESP32 Slaves, including power, operating mode, fan
  and target-temperature changes. This is field experience, not a general
  certification for other combinations.
- **ESPHome 2026.10.2 compilation (9–10 November 2026):** Standard Slave and
  Floor Console examples compiled successfully in Codespaces. Recorded flash
  usage was approximately 47.1% and 47.2% respectively. These were build
  results, not a claim that every later merged commit was independently
  recompiled.
- **Offline regression checks:** The initial integrated V2.0.0 baseline
  passed 19 C++ test programs; the later V2.0.4 fast-discharge/timing-fix
  work passed **22 C++ regression programs** and the Python syntax/UI checks.
  Run `bash scripts/run-v2-offline-tests.sh` against the current checkout.
- **Component naming:** The public implementation remains
  `esphome/components/lg_controller/`. New Floor Console YAML uses
  `floor_console_*`; legacy `floorstanding_*` keys are aliases and must not
  be mixed with their canonical equivalents.

## LG ARNU07GQAA4 Floor Console — experimental support

**Physical testing is continuing. Functions may fail to apply, respond slowly
or work differently from expected behaviour.** The result on this one model
does not validate other Floor Console models.

- **Observed (10 November 2026):** A Slave ESP32 transmitted normal `2D`
  settings commands for Top / Both / Bottom. A delayed guarded `28` full
  settings refresh later returned two matching genuine indoor `CD` reports
  confirming **Bottom**. Home Assistant displayed requested directions after
  UART echo in fast mode. Exact physical outlet response latency was **not**
  established from logs.
- **Strict mode:** Commands use genuine-source settings templates and
  confirm through indoor readback. Missing or stale templates may require a
  safe full-settings refresh first.
- **Fast discharge (optional):**
  `floor_console_fast_discharge: true` applies only to discharge direction.
  After a confirmed UART echo, Home Assistant shows the **requested but
  unverified** direction. After 45 seconds without another selection by
  default (or up to 180 seconds with the optional
  `floor_console_fast_verify_quiet_seconds`), a guarded, bus-dependent full
  settings refresh can check the latest target
  using two consistent indoor `CD` reports. Confirmation may take considerably
  longer than 45 seconds.
- **V2.0.5 scheduling experiment (not yet physically validated):** Longer
  quiet time and deferral during normal user controls may reduce the risk of
  a full-settings response blocking the next direction change. This cannot
  preempt a response that has already started. The absolute verification
  deadline remains bounded and a longer quiet time delays confirmation.
- **Inactive experiments:**
  `floor_console_passive_readback: false` and
  `floor_console_selective_cd_probe: false`. Passive 12-second monitoring
  and the unverified `2D.80` candidate did **not** deliver timely `CD` on
  this installation; `2D.80` has **not** been proven read-only.
- **Outstanding hardware checks:** Repeated physical Top/Bottom/Both outlet
  response timings, rapid changes, wall-controller overrides, power/mode/fan
  interactions, Comfort Cooling, Comfort Cooling Step, startup/reboot, sleep
  reservation interactions and long-term operation.

Leave the physical wall controller as Master for this arrangement; do not
rely on experimental console-specific controls for unattended operation.
See [Floor Console fast discharge](FLOOR_FAST_DISCHARGE_V204.md) and
[the historical investigation](research/FLOOR_CONSOLE_INVESTIGATION.md).

## Other limitations

- The Home Assistant sleep reservation input allows 0–480 minutes.
  Complete eight-hour set/readback/expiry operation is **not** fully
  confirmed; an earlier capture observed a genuine Master countdown at
  420 minutes. Model- and controller-specific cancellation behaviour may
  differ. See [sleep reservations](SLEEP_RESERVATIONS.md).
- Master-originated sleep edits, mode changes, reboots, zero cancellation
  and expiry need further cross-model testing.
- Installer functions, Auto Dry, vane controls, Jet/Power Heating and
  diagnostic polling are model-dependent and not all physically verified.
- The 100-baud example setting was tested on this installation, but is not
  established for every LG controller generation.
- Energy and power readings may be unavailable, static or misleading on
  particular units.
- Check pin assignments, ESP32 revision and network exposure before flashing.
  The example configurations may include an HTTP web server. Never commit
  `secrets.yaml`, personal logs or extracted LG controller firmware.

## Recommended before a tagged release

- [x] Test the V2.0.0 core controls on real LG hardware.
- [x] Compile the V2.0.0 standard and Floor Console examples with
  ESPHome 2026.10.2.
- [x] Run the V2.0.4 offline suite (22 C++ tests plus Python checks).
- [x] Observe delayed genuine indoor `CD` confirmation on ARNU07GQAA4.
- [ ] Compile the final `main` example configurations after the latest merge.
- [ ] Confirm repeated physical outlet movement and all model-specific
  functions across power and mode transitions.
- [ ] Review credential exclusions, public logs, attribution and licence.
