# V2 change history

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

Earlier iterative V1.x/Rev3 research preceded this published snapshot. Earlier development records remain available through Git history.
