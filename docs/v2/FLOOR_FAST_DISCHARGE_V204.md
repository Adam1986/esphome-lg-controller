# Floor Console V2.0.4: fast discharge / deferred verified readback (experimental)

## Why

The Garden Office unit accepts ordinary `2D` direction writes but does **not** produce prompt spontaneous `CD` reports. Forcing a full `28` settings refresh immediately after *every* direction click starts a long burst of `C8/C9/CA/CB/CD/CE` messages at 100 baud. During the burst, subsequent direction changes may queue for 40–60+ seconds.

Previous probes (`floor_console_passive_readback` and speculative `floor_console_selective_cd_probe`) both failed to yield an early `CD`, and added 10–12 seconds per attempt. Both must be set `false` for this experiment.

## V2.0.4 strategy

This is a **different UI trust model**, opt-in on the Garden Office only:

1. Send the unchanged, genuine-template-based `2D` direction write at the next safe bus slot (existing 500 ms idle rule), with no new packet types.
2. After **UART echo**, publish the **requested** direction to Home Assistant and release the transaction, explicitly marked `SENT (UNVERIFIED)` in the `Floor - Command Status` diagnostic. This is *optimistic*, not a statement of indoor acceptance.
3. If another choice arrives, transmit it at the next safe slot instead of waiting for the preceding selection's full-settings burst. Only the latest choice needs verification.
4. Once no further direction has been sent for **45 seconds**, issue **one** guarded, read-only `28` full-settings refresh using the existing current A8/C8 pairing, provided there is no active sleep reservation. This still takes a while to produce `CD`, but a group of selections causes only one expensive refresh.
5. Read two matching genuine indoor `CD` reports. If they match the requested direction, status becomes `CONFIRMED`. If two consistent reports disagree, publish the indoor's actual direction and status `REJECTED/OVERRIDDEN`. If no safe refresh or no indoor reports arrive within bounded time, report `UNVERIFIED` and leave the last requested direction visibly optimistic, subject to correction by a later genuine CD.
6. A newly changed physical wall-controller AD overrides a pending optimistic HA request. The 2D.80 probe is never transmitted in fast mode.

This **does not** speed up an individual outgoing 2D when the LG bus is already occupied. The main expected benefit is avoiding repeated ~minute-long full-settings responses when several direction selections occur in succession.

## Activation on Garden Office

```yaml
external_components:
  - source: github://Adam1986/esphome-lg-controller@experiment/floor-console-passive-cd
    components: [lg_controller]
    refresh: 10min

climate:
  - platform: lg_controller
    # ... existing configuration ...
    floor_console_features: true
    floor_console_monitoring: true
    floor_console_passive_readback: false
    floor_console_selective_cd_probe: false
    floor_console_fast_discharge: true
    floor_console_command_status:
      name: "Floor - Command Status"
      internal: false
```

This is deliberately not enabled by default; all other units keep existing strict confirmation.

## Live test

* With no active sleep timer and a physical controller available, choose Top, then Both, then Bottom around 4–8 seconds apart. Record physical outlet response, *not just* the HA selection.
* Look for the `FLOOR TX: page 5` and `FLOOR TX: SENT (UNVERIFIED)` messages, and ensure **no** `2D.80` or immediate `FLOOR READBACK: guarded 28` appears after each selection.
* After 45 seconds without another direction selection, look for `FLOOR FAST: one coalesced guarded 28 readback` and subsequently two genuine `CD` messages; confirm HA agrees with the actual physical outlet.
* Repeat with a single change, then observe the delayed confirmation. Do not interpret the immediate HA value as proof of indoor acceptance.
* If unexpected operation occurs, disable `floor_console_fast_discharge` and retain both older experimental flags as `false` to return to the original strict V2.0.1 behaviour.

## Caveats

The full settings poll still occupies the shared bus for a prolonged time once triggered; we have merely reduced its frequency. No verified selective Type-D read command has been identified. The delayed readback also depends on a safe, fresh Master/indoor Type-0 pair; it may be skipped during unsettled state or active sleep. This patch has offline unit and configuration checks but **no full ESPHome compile nor physical test** in the authoring environment.
