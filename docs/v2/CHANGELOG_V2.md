# V2 change history

The sections below record when functionality was developed. Inclusion in the
repository's `main` branch does **not** mean all model-specific features are
physically validated; see [testing and limitations](TESTING_AND_LIMITATIONS.md).

## 2026-11-11 — V2.0.5 experimental Floor Console verification scheduling

- Added optional `floor_console_fast_verify_quiet_seconds` (45–180, default
  **45**) for Slave Floor Consoles with fast discharge enabled. An experimental
  value of **120** allows more time for consecutive outlet changes before a
  full `28` settings response temporarily occupies the 100-baud bus.
- A user-initiated fan/mode/temperature/swing or Floor Console change resets
  only the *quiet clock*; an independent absolute deadline still prevents
  indefinite unverified status. Pending normal commands, installer writes,
  and fan calibration block the optional refresh.
- No changes to outgoing `2D` frames, 500 ms bus-idle rule, genuine indoor
  `CD` verification, normal Slave acknowledgements, or sleep guards.
- **Unverified on hardware** at the time of this patch; test on ARNU07GQAA4
  before relying on any latency improvements.

## 2026-11-11 — Documentation consolidation after merge

- Consolidated historical selective page-5 readback experiments into
  [Floor Console research](research/FLOOR_CONSOLE_INVESTIGATION.md).
- Removed superseded branch-merge instructions and experimental release notes.
- Updated `main` usage instructions, the V2.0.4 fast-discharge guide, and the
  explicit ARNU07GQAA4 compatibility warning. No firmware or YAML behaviour
  changed as part of this documentation cleanup.

## 2026-11-10 — V2.0.4 Floor Console fast discharge and timing fix

- Added opt-in `floor_console_fast_discharge` (off by default). A standard
  `2D` discharge-direction command is shown in Home Assistant on UART echo as
  **requested / unverified**, avoiding an immediate full-settings refresh
  after each selection.
- After 45 seconds without another direction command, a single guarded `28`
  refresh can check the latest direction through two genuine indoor `CD`
  reports; it remains subject to bus availability and sleep guards.
- Fixed a millisecond-timer underflow that incorrectly marked a just-echoed
  command unverified immediately, preventing the delayed refresh.
- **Observed on hardware:** Bottom was confirmed by matching indoor `CD`
  frames after delayed readback on an ARNU07GQAA4 in Slave mode. Immediate
  physical response and reliability across conditions remain unverified.
- The offline regression suite reached 22 passing C++ programs plus Python
  configuration/UI checks. These are not substitutes for hardware validation.
- See [fast-discharge documentation](FLOOR_FAST_DISCHARGE_V204.md).

## 2026-11-10 — V2.0.3 selective-CD candidate (unsuccessful experiment)

- Added an **opt-in, unverified** `2D.80` Type-D candidate following an
  ordinary `2D` write, with a 10-second response window and guarded full
  refresh fallback. This is **not a proven read-only request**.
- On-device tests produced UART echoes but no prompt matching indoor `CD`
  reports; the established `28` refresh was still required.
- Keep `floor_console_selective_cd_probe: false`. See
  [historical research](research/FLOOR_CONSOLE_INVESTIGATION.md).

## 2026-11-10 — V2.0.2 passive-CD experiment (unsuccessful)

- Added optional `floor_console_passive_readback` to wait 12 seconds for
  unsolicited `CD` after an ordinary `2D` write.
- The tested indoor unit did not produce timely `CD` reports within that
  window; the experiment delayed readback rather than improving it.
- Keep `floor_console_passive_readback: false`. Details are preserved in the
  [research archive](research/FLOOR_CONSOLE_INVESTIGATION.md).

## 2026-11-10 — V2.0.1 Floor Console naming and template recovery

- Standardised `floor_console_*` internal identifiers and YAML keys, with
  backward-compatible `floorstanding_*` aliases. Existing Home Assistant
  display names were preserved.
- When no suitable genuine `AD`/`CD`/`CE.10` template exists, the Floor
  Console path can request guarded full-settings readback before constructing
  a settings write. It does not fabricate missing settings pages.
- Retained bounded retry/scheduling, sleep reservation guards and genuine
  indoor acceptance checks for the strict control path.

## 2026-11-09 — V2.0.0 public fork integration

- Integrated the enhanced implementation at
  `esphome/components/lg_controller/`, retaining the upstream component name.
- Updated standard and Floor Console examples and their tests.
- 19 standalone C++ regression programs and Python checks passed for this
  baseline; both example configurations compiled with ESPHome 2026.10.2.
- The maintainer reported successful physical operation of normal power,
  mode, fan and temperature controls in Slave installations.
- Preserved upstream README, attribution and project licence.

## V2.0.0 feature baseline

- Improve Master/Slave synchronisation, command construction, arbitration,
  acknowledgements, retries and bounded status confirmation.
- Use 100-baud UART timing in supplied examples following LG driver
  research and on-device testing; it may not apply to every LG generation.
- Synchronise LG-managed sleep reservations, with an input range up to 480
  minutes; eight-hour end-to-end operation still requires validation.
- Extend diagnostic discovery, pipe-temperature policies and optional
  model-specific Floor Console controls.
