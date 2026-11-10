# Sleep reservation improvements

## Why the previous approach was unreliable

Earlier ESP32 handling largely relied on its own local countdown after requesting an LG sleep reservation. This could diverge from the physical wall controller if a request was ignored, cancelled externally or overridden. It also risked an extra ESP32-originated shutdown competing with the LG controller's built-in reservation expiry.

## Newer approach

The supplied V2 component has helpers for reservation set/cancel, Master state readback, countdown synchronisation and guarded diagnostic discovery (`lg_master_sleep.h`, `lg_slave_sleep_sync.h`, `lg_sleep_reservation.h`, `slave_pipe_temp_discovery.h`). The intended **source of truth** is the genuine physical Master status; Home Assistant should follow the reservation remaining on LG rather than simply assuming that a local timer is correct.

Observed in earlier on-device tests: a 420-minute reservation and Master reporting decrements such as 419, 418 and 417 minutes. Separate active-sleep diagnostic trials confirmed readbacks without changing the reservation on two ordinary indoor units. Tests also demonstrated successful cancellation in some conditions while the unit remained ON, although zero-minute cancellation and all state transitions are not universally proven.

## Duration limits

- Physical wall controllers in the tested installation commonly support **up to 480 minutes (8 hours)**.
- The supplied `climate.py` V2 component exposes `sleep_timer` with `min_value=0`, `max_value=480`, `step=1`.
- End-to-end ESP32 **480-minute set/readback/expiry** is **not yet established** from the archived V2 testing report. Do not conflate a UI maximum with validation of the full duration range.

## What still needs checking

- Master-originated sleep timer changes while Home Assistant is online.
- Explicit cancel to zero in Slave mode across multiple indoor models and operating states.
- Behaviour around mode changes (for example COOL → HEAT), power state changes, reboot and reservation expiry.
- Floor Console active-sleep discovery and the full eight-hour countdown.

## Tests and precautions

1. Set a modest reservation from Home Assistant and observe physical Master countdown/readback.
2. Change the reservation at the wall controller and check Home Assistant reflects the genuine Master.
3. Cancel from Home Assistant; confirm a genuine readback of zero **and** that the indoor unit remains ON if no shutdown was requested.
4. Confirm nothing turns the unit OFF independently of the LG reservation when the countdown expires.
5. Repeat across model types before describing a feature as generally reliable.
