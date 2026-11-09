# Protocol research notes — V2 development

> These notes separate observed traffic, LG source-derived transport insights and implementation hypotheses. They are not an official LG protocol specification.

## Transport layer

Existing community protocol documentation describes 104 baud. Investigation of available LG TTL communication-driver material identified a 500 µs timer tick and twenty ticks per bit, consistent with **100 baud**. The supplied V2 YAMLs use `baud_rate: 100`. This should be regarded as a finding tested on a particular hardware setup, not a universal prescription for every legacy LG device.

The research also gave insight into 6- and 13-byte packet handling, checksum behaviour (XOR with `0x55`), receive timing, and checking bus availability before transmission. The lower-level driver did not by itself explain the complete high-level Master/Slave setting-construction rules.

## Packet families examined

| Message | Observed / investigated role |
| --- | --- |
| `A8` | Master-controller status, including settings and reported reservation information |
| `C8` | Indoor-unit status; important for verifying command application |
| `28` | Slave-originated control/status requests |
| `2C` | Additional Slave communication seen in genuine controller captures; precise model-dependent semantics need care |
| `CA`, `CB`, `CE.10` | Extended indoor/installer data readbacks used in capability discovery |
| `2B.80` | Diagnostic query associated with pipe-temperature retrieval in captured tests |
| `CC` | Status information including filter/operating data on supported units |

## Important implementation principles

- Transmitting a valid frame (or seeing a loopback echo) **does not prove** the indoor unit has adopted the change.
- Prefer preserving the latest genuine Master template, and confirm state changes using later genuine indoor and Master reports.
- Avoid constructing unsupported Type-A settings packets before the genuine indoor `CA` template is available.
- Command timing, queuing and retries need to account for slow bus traffic and controller arbitration.
- Differences between indoor model families and wired-controller revisions may remain significant.

## Research provenance

Investigation combined existing project protocol documentation, publicly available LG low-level GPL-driver information, study of controller firmware, real serial logs and iterative AI-assisted coding/testing. **No extracted firmware binary or proprietary controller source is redistributed in this overlay.**
