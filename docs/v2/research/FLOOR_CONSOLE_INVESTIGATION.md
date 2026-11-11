# Floor Console page-5 readback — historical research (10 November 2026)

**Historical record, not an installation or test procedure.** This page
consolidates the V2.0.2 passive-`CD` observation and V2.0.3 speculative
Type-D selective-read experiments. Both were superseded by the optional
[V2.0.4 fast-discharge approach](../FLOOR_FAST_DISCHARGE_V204.md).

## Why readback was investigated

Standard mode, temperature and fan changes use `28` settings writes and
regular indoor `C8` status updates. Floor Console Top/Bottom/Both changes
use page-5 `2D` writes, but the tested `ARNU07GQAA4` did not report a
corresponding indoor `CD` promptly without a full `28` settings refresh.
That refresh can return extended `C8/C9/CA/CB/CD/CE` messages over a
100-baud bus, creating significant command-queue delays.

## Firmware and protocol clues

- The examined PREMTB100 V2.12.3a `server_target` ELF had build ID
  `c28fa4623a3af913d2df3200e50c7d83db4d9fe1`. Routines named
  `processCntForSendCMD5` and `LoadTxdataTo5Page13byte` relate to
  page-5 **settings transmissions**; they did not establish a page-5
  read-only request.
- An examined `AccessProtocol5Page` routine at Thumb address `0x1f695`
  handled byte-1 bits 0–6 but did not set bit 7. This was only an
  observation from the extracted firmware, **not evidence that bit 7 is
  a read flag for Type-D packets**.
- Existing [community protocol documentation](../../../protocol.md)
  identifies `AB[1] & 0x80` in Type-B as a read request for indoor `CB`.
  That convention does not establish the same meaning for Type-D `2D`.

## V2.0.2 — passive `CD` experiment

`floor_console_passive_readback: true` waited for two spontaneous indoor
`CD` reports for 12 seconds following an ordinary `2D` write, before
falling back to the established full-settings `28` refresh.

**Observed:** On Garden Office, the indoor unit did not produce matching
`CD` reports within the 12-second window during the recorded tests. Full
settings readback was still needed and the extra wait made one-off changes
slower. The option is **off by default** and should remain disabled for
this tested unit.

## V2.0.3 — speculative Type-D `2D.80` candidate

The test cloned the genuine, transmitted `2D` settings frame, set only
byte 1's `0x80` bit, and recalculated the checksum. One observed Bottom
example was:

```text
Normal direction write: 2D.00.A5.0A.20.04.00.00.02.00.00.00.57
Unverified candidate:   2D.80.A5.0A.20.04.00.00.02.00.00.00.D7
```

**Observed:** Six `2D.80` transmissions echoed locally, but none produced
a matching indoor `CD` confirmation within the 10-second probe window.
The guarded `28` refresh remained necessary; later genuine indoor `CD`
reports confirmed the intended discharge directions. UART echo was **not**
proof of indoor acceptance or a selective-read response.

**Safety limit:** `2D.80` is **not established as read-only**. It may be
interpreted as a settings write or another undocumented operation. Keep
`floor_console_selective_cd_probe: false`. Do not use this speculative
command on other models based on the unsuccessful trial.

## Current conclusion

Neither experiment demonstrated a timely page-5 selective read. The
merged component retains the opt-in flags for research compatibility, but
they are disabled in examples. The current V2.0.4 alternative sends a
normal `2D` write and delays **one coalesced, guarded** full settings
refresh. A real Garden Office trial confirmed a Bottom selection through
two matching indoor `CD` reports after that deferred refresh. Physical
outlet response times and other special features remain to be tested.
