# Floor Console V2.0.4 — fast discharge and delayed readback (experimental)

> **LG ARNU07GQAA4 compatibility warning:** These Floor Console controls
> are still being tested on hardware. Initial tests demonstrate ordinary
> `2D` discharge commands and genuine delayed indoor `CD` confirmation, but
> do not establish reliability in all operating states or immediate physical
> outlet movement. Use with a genuine LG wall Master and supervise initial
> tests.

## Why this exists

Temperature, mode and fan-speed settings use normal status messages (`28`
and indoor `C8`) and are reported routinely. The special Floor Console
Top/Bottom/Both setting uses page-5 `2D` writes and `CD` readback instead.
On the tested ARNU07GQAA4, `CD` was not reported promptly without a full
`28` settings refresh. Repeating that refresh after every direction change
occupied the 100-baud bus and delayed subsequent commands.

## How fast discharge works

1. Send the ordinary, checksum-validated `2D` discharge command from a
   genuine LG settings template at the next safe bus opportunity. No new
   on-wire opcode is introduced.
2. After its **UART echo**, release the direction transaction and publish
   the **requested (not yet verified)** selection in Home Assistant. The
   command-status entity reports `SENT (UNVERIFIED)`. A UART echo is not proof
   that the physical indoor unit accepted the command.
3. Further direction selections can be queued without waiting for an
   immediate full settings response after each click.
4. After **45 seconds** without another direction transmission, schedule
   **one** guarded full-settings `28` readback when the shared bus and
   sleep-reservation safeguards permit it. The actual send may occur later.
5. Compare two consistent genuine indoor `CD` messages with the most recent
   requested direction. Report `CONFIRMED` on a match; otherwise correct
   the displayed direction or report the outcome as unverified according to
   the received status and timeout guards.

The normal 500 ms bus-idle checks, source-template validation and safety
guards are retained. The feature does not speed up an outgoing `2D` during
an already-running LG full-settings exchange.

## Configuration

In a Floor Console device that uses the merged component on `main`:

```yaml
external_components:
  - source: github://Adam1986/esphome-lg-controller@main
    components: [lg_controller]
    refresh: 10min

climate:
  - platform: lg_controller
    # Keep your existing device-specific settings and controls here
    floor_console_features: true
    floor_console_monitoring: true
    floor_console_passive_readback: false
    floor_console_selective_cd_probe: false
    floor_console_fast_discharge: true  # Experimental, opt-in
    floor_console_command_status:
      name: "Floor - Command Status"
      internal: false
```

This is an **illustrative fragment**, not a complete ESPHome configuration.
Keep your own pins, substitutions, Master/Slave setting, and source-template
configuration. The repository's shared Floor Console example leaves fast
discharge **off by default**.

## Evidence from the 10 November 2026 trial

- The initial fast-discharge build contained a loop-timestamp ordering
  defect: a UART echo could be processed after the scheduler had captured
  its time, causing unsigned timer underflow and immediate `UNVERIFIED`
  status. The subsequent patch corrected that defect.
- Following the fix, a **Bottom** direction command was queued at
  **23:37:08**, sent at **23:37:14**, and echoed / shown as requested by
  **23:37:16**.
- The one delayed guarded settings refresh started at **23:38:11**.
  Matching genuine indoor `CD` reports arrived at **23:38:34** and
  **23:38:37**, and command status became `CONFIRMED`.
- This verifies the deferred-confirmation mechanism for that test. It
  does **not** quantify physical outlet movement speed or prove every
  command succeeds. Additional physical tests remain necessary.

## Practical testing and fallback

Start with no active sleep reservation and somebody present at the unit.
Change Top / Both / Bottom one at a time, observe the physical discharge
outlets, and compare the timing with `FLOOR TX` and `CD` messages. Leave the
unit untouched long enough for a delayed refresh to complete.

If behaviour is unexpected, disable `floor_console_fast_discharge` and keep
`floor_console_passive_readback` and `floor_console_selective_cd_probe`
set to `false`. This restores the strict Floor Console readback path, while
retaining normal bus and sleep guards.

The unsuccessful `2D.80` selective request is **not** validated as a
read-only command and should not be enabled. Historical evidence is recorded
in [Floor Console research](research/FLOOR_CONSOLE_INVESTIGATION.md).
