# Floor Console page-5 selective read investigation — 10 November 2026

## Evidence and limitations

- Extracted PREMTB100 V2.12.3a `server_target` ELF, build ID `c28fa4623a3af913d2df3200e50c7d83db4d9fe1`: `processCntForSendCMD5` and `LoadTxdataTo5Page13byte` are page-5 *settings transmissions*, not demonstrated selective read requests.
- `protocol.md` documents a read flag `AB[1] & 0x80` for Type-B (`CB`) only. There is no proof of a read-only `2D.80` request for Type-D (`CD`). Never send a speculative `2D.80` as if it were verified.
- In the 10 November 19:16–19:18 capture, a guarded `28` readback returns a long diagnostic burst and delays another `2D` command by ~53 seconds.

## Opt-in test implemented here

`floor_console_passive_readback: true` waits 12 seconds after the normal genuine-template-based Slave `2D` direction command for two independent spontaneous indoor `CD` reports. If none arrive, the existing guarded `28` full-settings request is used as fallback. A newer HA selection during the passive window supersedes an older unconfirmed request before a full-settings burst is queued.

All existing limits remain: sleep reservation guards, 500 ms idle-bus check, genuine source templates, two indoor `CD` confirmation messages, bounded write retry, and no new speculative UART opcode. Default is false; only the floor example enables the experiment. Other controls are unchanged.

**Important:** If the indoor unit only emits `CD` in response to a full `28` request, this experiment adds about 9.5 seconds to one-off changes and is NOT a performance improvement. Disable the YAML option if all attempts fall back. Testing cannot prove a targeted request exists.

## Test

1. Flash Garden Office only after a full ESPHome compile with this component.
2. Ensure no sleep reservation is active. Make a single Top or Both change and observe `FLOOR PASSIVE`, `FLOOR TX`, `FLOOR READBACK` and `CD` logs.
3. Determine whether two real `CD` frames arrived **without** an intervening `28`, or whether the fallback always triggers.
4. Try quick Top → Bottom → Both changes within 12 seconds; verify only the latest requested selection gets a new `2D` before full readback.
5. Confirm physical outlets and HA displayed value after confirmation. Keep on test branch; do not merge into stable main yet.
