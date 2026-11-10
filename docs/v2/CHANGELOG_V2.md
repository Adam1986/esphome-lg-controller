## Unreleased — V2.0.2 Floor Console passive-first readback experiment

- Optional `floor_console_passive_readback` (off by default; on in the floor
  example) waits up to 12 seconds for spontaneous genuine indoor `CD` after
  a `2D` discharge write, before the existing guarded `28` full refresh.
- Allows rapid selections to supersede an unverified direction without
  launching a full-settings burst immediately; full fallback remains.
- No speculative `2D.80` selective request or relaxation of collision/acceptance guards.
- Investigation: `docs/v2/SELECTIVE_PAGE5_INVESTIGATION.md`.

# V2 change history

## Unreleased — Floor Console read-before-write recovery (V2.0.1 candidate)

- Based directly on the `feature/floor-console-naming` source export. Preserve the
  canonical `floor_console_*` naming and backward-compatible `floorstanding_*` aliases.
- When a Floor Console command lacks a recent, genuine `AD`/`CD`/`CE.10`
  template, queue a **guarded, read-only `28` full-settings request** before
  trying any `2C`/`2D`/`2E` command. Use the existing matching A8+C8 rules
  and retain the no-active-reservation policy for Floor Consoles.
- Accept only a real, checksum-valid, age-checked page as the source of the
  subsequent targeted command. Never fabricate page contents.
- Limit template refresh to two queries and the existing transaction deadline;
  report failure without a settings write if the page does not arrive.
- Floor Console switch/select requests no longer optimistically publish their
  state to Home Assistant: genuine observed readback owns the displayed state.
  Latest-choice queue, physical Master precedence and two indoor confirmations
  are retained.
- Add regression coverage for query construction, timer/mismatched-status
  guards, bounded refresh scheduling and non-optimistic UI controls.
- **Validation outstanding:** full ESPHome compile of renamed component,
  one physical Garden Office test and eventual GitHub branch merge.
  See [merge instructions](MERGE_V2_0_1.md).

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
