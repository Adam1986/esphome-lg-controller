# V2.0.3 — experimental Type-D selective `CD` read (NOT protocol-verified)

## Why

The V2.0.2 12-second passive observation repeatedly fell back to a full `28` settings refresh on Garden Office. Long `CA/CB/CD/CE.*` response bursts left the UART occupied for nearly a minute and made subsequent changes sluggish. Disable `floor_console_passive_readback` for this trial.

## Actual reverse-engineering evidence

* The extracted PREMTB100 V2.12.3a ELF `server_target`, `AccessProtocol5Page` at `0x1f695` (Thumb), handles byte 1's settings bits 0–6 (at global buffer `0x1007bc+0x3d`); it does not set bit 7 in this routine.
* The documented Type-B `AB[1] & 0x80` bit *does* request an indoor `CB` read. It is a clue, **not proof** that Type-D uses this flag.
* `processCntForSendCMD5` and `LoadTxdataTo5Page13byte` prove Type-5 **writes**, not an independent Type-5 read mechanism.

## Candidate and risk

With a normal, checksum-correct Slave `2D` discharge-direction command already sent, we clone the **exact transmitted bytes**, set byte 1 bit 7 and recalculate the checksum. For a captured Bottom command:

```
Ordinary write:   2D.00.A5.0A.20.04.00.00.02.00.00.00.57
Candidate probe:  2D.80.A5.0A.20.04.00.00.02.00.00.00.D7
                          ^ only payload bit changed
```

**WARNING: The candidate is unverified. It might be interpreted as a write or have other unintended effects on installer settings.** Retaining all existing settings bytes, including the requested direction, limits *but does not eliminate* risk. Before trying a physical unit, ensure normal operation, preferably with no active reservations; have the genuine LG wall controller available, and be prepared to revert to V2.0.1. This packet must not be used on standard units or in a stable build.

## Implementation

* New opt-in `floor_console_selective_cd_probe: true`; default `false`.
* Only Slave floor console, only following a manually requested discharge-direction command with a genuine source template, only after its UART echo; one candidate per write, never on boot and never during active sleep/reservation.
* Uses the normal bus-idle/priority checks; it is **not** a fabricated `CD` or an automatic settings write retry.
* Waits 10 seconds after the candidate for two genuine matching indoor `CD` reports. If none arrive, tries the established, guarded full `28` settings refresh. A late `CD` is not falsely attributed to the probe.
* Leaves the Home Assistant select showing last confirmed state until genuine indoor `CD` confirmation.
* Does not alter normal fan, temperature, mode or sleep-control writes.

## How to run the test (reuse the existing experimental branch)

On Garden Office's floor console base (test branch only):

```yaml
external_components:
  - source: github://Adam1986/esphome-lg-controller@experiment/floor-console-passive-cd
    components: [lg_controller]
    refresh: 10min

climate:
  - platform: lg_controller
    # other existing configuration unchanged
    floor_console_passive_readback: false
    floor_console_selective_cd_probe: true
```

1. Compile and deploy to Garden Office only. Do not change sleeping units or other room firmwares.
2. With the unit in a normal operating state and no active sleep timer, request **Top**, leave it alone for the entire confirmation cycle; then repeat for Both and Bottom.
3. Record physical airflow change and exact timestamps for `FLOOR TX`, `FLOOR SELECTIVE EXPERIMENT`, `received 2D.80`, `received CD`, `FLOOR TX: CONFIRMED`, and any `FLOOR READBACK: guarded 28` fallback.
4. If the probe yields two `CD` frames without a `28` and no unrelated values change, repeat once to rule out coincidence. If no `CD` arrives, turn the YAML option off; this candidate is **not** supported on that unit.
5. If the indoor unit behaves unexpectedly, stop testing, revert firmware, and verify all installer settings/operation using the genuine Master.

## Interpretation limits

A UART echo only proves transmission. A `CD` received immediately after the probe is promising, but a coincidental or already queued message must be ruled out with repeated tests and timestamps. Even a responsive `CD` does not by itself prove that `2D.80` is semantically read-only; monitor for unrelated settings changes and avoid production use until demonstrated.

### Apply through Codespaces without creating another branch

```bash
cd /workspaces/esphome-lg-controller
git switch experiment/floor-console-passive-cd
git pull --ff-only origin experiment/floor-console-passive-cd
git am LG-Controller-V2.0.3-Selective-CD-Probe.patch
git push origin experiment/floor-console-passive-cd
```

Then leave `external_components` on this same branch. Switch the two YAML flags as shown, compile the actual Garden Office YAML and flash only that device.
