# V2 change history

## Unreleased — Floor Console naming consistency

- Rename internal Floor Console header files, C++ identifiers, and regression-test files
  from `floorstanding` to `floor_console`, matching the existing public YAML filenames.
- Use `floor_console_*` configuration keys in the standard and console bases.
- Keep the previous `floorstanding_*` configuration keys as backward-compatible aliases,
  with an explicit error if both names for one setting are supplied.
- Preserve existing Home Assistant entity display names and LG bus message behaviour.
- Offline regression checks pass; a full ESPHome compile of this refactor remains pending.

## 2026-11-09 — V2.0.0 public fork integration

- Integrated the working V2.0.0 implementation directly into the original `esphome/components/lg_controller/` component.
- Updated standard/floor console sample configurations and regression tests to reference the public `lg_controller` component name.
- Confirmed 19 C++ regression programs, Python syntax and UI-guard tests pass following integration.
- Successfully validated and compiled the standard Slave example under ESPHome **2026.10.2** in GitHub Codespaces (47.1% flash use).
- Recorded maintainer's successful V2.0.0 physical deployment across their LG installation.
- Preserved original upstream README/history/licence and credited the intermediate balazsborbely fork.

## V2.0.0 feature baseline

- Improve Master/Slave state synchronisation, command construction, pending-change arbitration, acknowledgements and retries.
- Use 100-baud UART timing in supplied example bases following the LG driver timing investigation.
- Add Master-driven sleep reservation readback, set/cancel helpers, sleep power-off coordination and guarded diagnostic discovery.
- Expose up to 480 minutes in the Home Assistant sleep input, with full-duration physical validation still pending.
- Improve extended data/capability discovery, pipe-temperature policies, optional floor console control paths and related diagnostics.

V2.0.0 represents the consolidated result of earlier local development, protocol investigation and hardware testing. The repository contains the enhanced implementation, associated regression tests and documentation of the relevant findings. Earlier experimental builds and intermediate development versions are not included.
