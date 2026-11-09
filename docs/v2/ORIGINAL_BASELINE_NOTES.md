# LG Controller V2.0.0 — Shared Baseline Candidate

**9 standard LG indoor units + 1 Garden Office floor-standing unit.**  All units use one `components/lg_controller_rev3` source tree and one of two complete base YAMLs.  This package is the proposed V2 baseline for further enhancements.

## Changes from the V1.8.8 sleep-safe experimental build

1. **Sleep-safe installer discovery is now ON by default for standard Slave units.** The `slave_sleep_safe_discovery` YAML option still exists with default `true`, but neither base YAML contains the line. In particular, no separate experimental flag needs adding to individual standard units.
2. **Two real, successful active-sleep trials** motivated the new standard default: Porch (reservation counted down 416→415→414; indoor CA and CB retrieved, then a live 2B.80 pipe refresh succeeded) and Downstairs Bedroom (417→416→414→413→412; CA, CB, CE.10 retrieved). In both tests the ESP sent no sleep-control command as part of discovery, and no cancellation/reset was observed. This does **not** prove safety for every possible indoor model or reservation type.
3. **Garden Office keeps the existing conservative active-sleep policy.** The revised request is only used there for the ordinary no-reservation bootstrap until its special floor-standing + active-sleep combination is tested. Floor features, reversed airflow, filter hours, queue and readback code are retained as before.
4. **Auto Dry / vane write preflight.** A user/HA Type-A change sent to a Slave before the first genuine indoor `CA` packet is now **rejected before optimistic switch/select state publication**. The command does NOT transmit and is NOT retained to replay later. The existing startup discovery will retrieve the `CA`; the user/automation must issue a new Auto Dry/vane command after that. This avoids false HA success and stale delayed commands unexpectedly overriding a newer wall-controller decision.
5. **Second protection at actual Type-A send time.** If the genuine indoor `CA` template is absent (or has been replaced by a Master `AA`) by the time the bus is ready, no Type-A settings packet is constructed or transmitted. The Master-mode path is otherwise unchanged.
6. **Version identity in logs:** `LG CONTROLLER V2.0.0 baseline...` during ESP32 setup.

## Files

- `components/lg_controller_rev3/`: complete shared enhanced ESPHome external component
- `base-lg-ac-rev3.yaml`: complete standard base for the nine non-floor-standing units; includes Filter Time Remaining
- `base-lg-ac-floorstanding.yaml`: complete Garden Office floor-standing base; floor controls and all prior diagnostic visibility retained
- `tests/`: original 18 standalone C++ tests, V2 Type-A preflight test, and mock-ESPHome UI guard integration test
- `CHANGES_FROM_PREVIOUS.patch`: textual patch against the preceding experimental archive
- `TEST_RESULTS.txt`: validation performed during preparation

## Installing

1. Back up the existing `/config/components/lg_controller_rev3/` and both base YAML files.
2. Copy the packaged `components/lg_controller_rev3/` folder into `/config/components/lg_controller_rev3/` (replace the existing shared component folder).
3. Replace `/config/base-lg-ac-rev3.yaml` and `/config/base-lg-ac-floorstanding.yaml` with the two full YAMLs supplied here. Existing individual per-room YAMLs retain their same includes and substitutions.
4. Compile and install **one standard room first** (e.g. Porch or Downstairs Bedroom, already used for active-sleep discovery trials), and check its logs before continuing to the other standard units. Changing the component files does not change running devices until their ESP32 firmware is recompiled and installed.
5. **Garden Office requires no functional migration.** Only recompile it when you wish to adopt V2 source. If you leave its current firmware running, it stays unchanged. Its active-sleep bootstrap remains on the earlier conservative policy even after V2 is installed.
6. Check the wall-controller sleep countdown and indoor CA/CB/CE.10 readback. For a standard room with active sleep, expected lines include `SLAVE PIPE DISCOVERY V2`, `sending 28...`, `indoor CB received`, and `SLAVE DIAGNOSTIC SLEEP CHECK: genuine Master STILL reports active reservation`.
7. Test an Auto Dry or vane change **after** the indoor `CA` has arrived. Before `CA`, a V2 warning explicitly rejects the change; after `CA`, the existing Type-A send logic is used. There is no new claim of indoor acceptance without later readback.

## Safety / rollback

- Do not confuse `verified send` with indoor acceptance. Only genuine CA/CB/C8 etc. show indoor responses.
- The new default allows the same specific active-sleep query used in two successful physical trials, not arbitrary timer writes. The wall controller remains Master.
- For a temporary rollback of standard-unit sleep-safe bootstrap, you can explicitly set `slave_sleep_safe_discovery: false` **in that unit's climate configuration or a temporary dedicated base**. The option remains supported although the common bases no longer list it.
- To undo V2 completely, restore the backed-up component and bases and recompile the affected ESP32 devices.

## Verification and limits

- 19 standalone C++ test programs passed, including the existing protocol tests and new Type-A preflight test. The **actual** `LgSwitch` / `LgSelect` V2 implementations were compiled and tested against mock ESPHome base classes (separate test).
- YAML and Python syntax checks passed; both bases still register the shared component and filter life sensor.
- A full ESPHome 2026.10.2 compile **could not be performed** here (ESPHome is not installed in the container). Physical tests with this precise V2 package are still required before treating it as proven on all ten devices. The new pre-CA Type-A guard has not yet been live-tested.
