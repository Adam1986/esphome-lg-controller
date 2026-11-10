# Sleep Reservation Handling — V2.0.0

## Background and existing protocol research

The original project's [protocol documentation](../../protocol.md)
already describes how LG sleep reservations operate.

In particular, it documents:

- Reservation types and durations encoded in bytes 8 and 9.
- Sleep reservations using type 3.
- The physical controller regularly transmitting updated remaining minutes.
- The distinction between reservation durations and reservation status flags.
- The physical wired controller's responsibility for reservation expiry.
- Sleep-specific operating behaviour that cannot be replicated by simply
  scheduling a Home Assistant power-off automation.

These are findings from the original project and should not be attributed
to the V2 development.

The V2 improvements focus on using this existing protocol knowledge to
provide more reliable operation when an ESP32 is configured as a Slave
alongside a genuine LG Master controller.

## Improvements introduced in V2

### 1. Master-authoritative countdown synchronisation

The ESP32 uses genuine Master A8 messages to establish and periodically
correct the remaining sleep reservation duration displayed in Home Assistant.

A local countdown can still be used between Master reports, but it is
synchronised against the Master's reported duration rather than treated
as an independent, authoritative reservation.

Testing demonstrated a 420-minute reservation counting down through
419, 418 and 417 minutes in subsequent Master messages.

```yaml
[14:03:02.669][D][lg-controller:2422]: received A8.12.00.12.40.44.D1.16.19.1C.04.00.25 (13)
[14:03:02.674][I][lg-controller:1674]: SLAVE SLEEP: genuine Master reports 284 minutes remaining
[14:03:02.778][S][number]: 'Sleep Timer (minutes)' >> 284.00
[14:04:20.681][D][lg-controller:2422]: received A8.12.00.12.40.44.D1.16.19.1B.04.00.3A (13)
[14:04:20.682][I][lg-controller:1674]: SLAVE SLEEP: genuine Master reports 283 minutes remaining
[14:04:20.786][S][number]: 'Sleep Timer (minutes)' >> 283.00
[14:05:10.663][D][lg-controller:2422]: received A8.12.00.12.40.44.D1.16.19.1A.04.00.3B (13)
[14:05:10.718][I][lg-controller:1674]: SLAVE SLEEP: genuine Master reports 282 minutes remaining
```

### 2. Correct interpretation of reservation formats

LG supports different representations of reservation information.

The V2 decoder distinguishes between actual duration messages and
reservation-status flag messages.

For example, the sequence 18.10.84 describes reservation flags and
must not be interpreted as a 16-minute sleep reservation.

This distinction prevents incorrect countdown values being exposed
to Home Assistant.

### 3. Confirmation of reservation changes

Successfully transmitting a reservation request does not establish
that the physical LG system has accepted it.

V2 checks subsequent messages from both:

- A8: Genuine Master status, including actual reservation duration.
- C8: Indoor-unit status, including reservation-active information.

The Master's duration readback is required to verify the requested
reservation length. The indoor unit's active flag alone cannot establish
the exact remaining duration.

```yaml
[14:09:02.177][D][lg-controller:1784]: Setting sleep timer: 420 minutes
[14:09:02.180][I][lg-controller:1819]: SLAVE sleep reservation request queued (420 minutes)
[14:09:02.280][S][number]: 'Sleep Timer (minutes)' >> 420.00
[14:09:02.397][D][lg-controller:2422]: received CF.02.00.00.00.00.00.00.3A.19.01.00.70 (13)
[14:09:02.534][S][sensor]: 'Current Power' >> 0.0 kW
[14:09:03.860][D][lg-controller:2422]: received CE.23.00.00.00.00.00.00.00.00.00.00.A4 (13)
[14:09:05.313][D][lg-controller:2422]: received CE.23.00.00.00.00.00.00.00.00.00.00.A4 (13)
[14:09:06.773][D][lg-controller:2422]: received A8.12.00.02.40.44.D1.16.00.00.04.00.7E (13)
[14:09:06.780][D][lg-controller:2865]: ignoring incoming settings changes because of an outgoing pending change
[14:09:08.175][D][lg-controller:2422]: received AC.9D.08.00.00.15.10.10.00.2A.00.08.ED (13)
[14:09:09.675][D][lg-controller:2422]: received AC.9D.08.00.00.15.10.10.00.2A.00.08.ED (13)
[14:09:10.201][I][lg-controller:2065]: SLAVE SLEEP: transmitting explicit type-3 set (420 min); waiting for Master duration + indoor state
[14:09:10.201][D][lg-controller:2085]: sending 28.13.00.12.40.44.D1.16.19.A4.04.00.2C (13)
[14:09:10.214][I][lg-controller:2102]: SLAVE first transmission after 8019 ms from queued command
[14:09:10.214][I][lg-controller:2105]: SLAVE TX attempt 1/3 (awaiting indoor 0xC8, not UART echo)
[14:09:11.496][D][lg-controller:2422]: received 28.13.00.12.40.44.D1.16.19.A4.04.00.2C (13)
[14:09:11.496][D][lg-controller:2437]: verified send
[14:09:11.500][D][lg-controller:2475]: SLAVE UART echo received; still awaiting indoor acceptance
[14:09:30.715][D][lg-controller:2422]: received A8.12.00.12.40.44.D1.16.19.A4.04.00.AD (13)
[14:09:30.716][I][lg-controller:1674]: SLAVE SLEEP: genuine Master reports 420 minutes remaining
[14:09:30.716][I][lg-controller:1713]: SLAVE SLEEP: genuine Master A8 confirms sleep duration (420 min requested)
[14:09:30.724][I][lg-controller:2619]: SLAVE Master 0xA8 now reflects request (20517 ms since last TX); awaiting indoor 0xC8
[14:09:30.724][D][lg-controller:2865]: ignoring incoming settings changes because of an outgoing pending change
[14:09:32.217][D][lg-controller:2422]: received A8.12.00.12.40.44.D1.16.19.10.84.00.B1 (13)
[14:09:32.217][D][lg-controller:2865]: ignoring incoming settings changes because of an outgoing pending change
[14:09:33.717][D][lg-controller:2422]: received AC.9D.08.00.00.15.10.10.00.2A.00.08.ED (13)
[14:09:35.218][D][lg-controller:2422]: received AC.9D.08.00.00.15.10.10.00.2A.00.08.ED (13)
[14:09:36.820][D][lg-controller:2422]: received AE.80.3A.00.67.00.01.D2.02.00.15.01.EF (13)
[14:09:36.920][S][sensor]: 'Humidity' >> 58 %
[14:09:36.920][S][sensor]: 'Fan Operation Time' >> 103 h
[14:09:36.920][S][sensor]: 'Indoor Unit Operation Time' >> 466 h
[14:09:36.920][S][sensor]: 'Precise Room Temperature' >> 21.1 °C
[14:09:38.322][D][lg-controller:2422]: received AE.80.3A.00.67.00.01.D2.02.00.15.01.EF (13)
[14:09:38.440][S][sensor]: 'Humidity' >> 58 %
[14:09:38.441][S][sensor]: 'Fan Operation Time' >> 103 h
[14:09:38.441][S][sensor]: 'Indoor Unit Operation Time' >> 466 h
[14:09:38.441][S][sensor]: 'Precise Room Temperature' >> 21.1 °C
[14:09:58.652][D][lg-controller:2422]: received A8.12.00.12.40.44.D1.16.19.A3.04.00.A2 (13)
[14:09:58.697][I][lg-controller:1674]: SLAVE SLEEP: genuine Master reports 419 minutes remaining
[14:09:58.697][D][lg-controller:2865]: ignoring incoming settings changes because of an outgoing pending change
[14:10:00.150][D][lg-controller:2422]: received A8.12.00.12.40.44.D1.16.19.10.84.00.B1 (13)
[14:10:00.154][D][lg-controller:2865]: ignoring incoming settings changes because of an outgoing pending change
[14:10:01.651][D][lg-controller:2422]: received AC.9D.08.00.00.15.10.10.00.2A.00.08.ED (13)
[14:10:03.253][D][lg-controller:2422]: received AC.9D.08.00.00.15.10.10.00.2A.00.08.ED (13)
[14:10:05.664][D][lg-controller:2422]: received C8.12.00.12.40.04.D1.1E.00.00.00.00.4A (13)
[14:10:05.667][I][lg-controller:2626]: SLAVE first matching indoor 0xC8; waiting for second consistent 0xC8
[14:10:05.667][D][lg-controller:2865]: ignoring incoming settings changes because of an outgoing pending change
[14:10:07.159][D][lg-controller:2422]: received C8.12.00.12.40.04.D1.1E.00.00.00.00.4A (13)
[14:10:07.163][I][lg-controller:2640]: SLAVE SLEEP SET CONFIRMED: 420-minute request accepted (Master A8 and two consistent indoor C8 status reports)
[14:10:07.167][I][lg-controller:2649]: SLAVE TX CONFIRMED by two indoor 0xC8 reports after 1 attempt(s) (56962 ms after TX)
```

### 4. Handling external changes

The implementation monitors reservation information originating from
the physical Master.

Changes made directly at the wired controller can therefore be reflected
in Home Assistant.

Additional logic distinguishes delayed acknowledgements from genuine
external changes, reducing the risk of stale ESP32 requests overriding
more recent Master settings.

### 5. Cancellation handling

Cancellation uses an explicit type-3 reservation request with zero minutes.

The implementation subsequently checks the reported LG reservation state
rather than treating transmission alone as proof of cancellation.

Tests have demonstrated successful cancellation while the indoor unit
remains powered ON, although not every model and operating state has
been fully validated.

```yaml
[14:08:05.958][D][lg-controller:1784]: Setting sleep timer: 0 minutes
[14:08:05.958][I][lg-controller:1819]: SLAVE sleep reservation request queued (0 minutes)
[14:08:06.064][S][number]: 'Sleep Timer (minutes)' >> 0.00
[14:08:08.897][I][lg-controller:2065]: SLAVE SLEEP: transmitting explicit type-3 cancel (0 min); waiting for Master duration + indoor state
[14:08:08.900][D][lg-controller:2085]: sending 28.13.00.02.40.44.D1.16.18.00.04.00.91 (13)
[14:08:08.900][I][lg-controller:2102]: SLAVE first transmission after 2939 ms from queued command
[14:08:08.900][I][lg-controller:2105]: SLAVE TX attempt 1/3 (awaiting indoor 0xC8, not UART echo)
[14:08:10.206][D][lg-controller:2422]: received 28.13.00.02.40.44.D1.16.18.00.04.00.91 (13)
[14:08:10.209][D][lg-controller:2437]: verified send
[14:08:10.209][D][lg-controller:2475]: SLAVE UART echo received; still awaiting indoor acceptance
[14:08:28.722][D][lg-controller:2422]: received A8.12.00.02.40.44.D1.16.18.00.04.00.16 (13)
[14:08:28.723][I][lg-controller:1677]: SLAVE SLEEP: genuine Master reports reservation cleared
[14:08:28.723][I][lg-controller:1713]: SLAVE SLEEP: genuine Master A8 confirms cancellation (0 min requested)
[14:08:28.723][I][lg-controller:2619]: SLAVE Master 0xA8 now reflects request (19825 ms since last TX); awaiting indoor 0xC8
[14:08:55.112][D][lg-controller:2422]: received C8.12.00.02.40.04.D1.1E.00.00.00.00.5A (13)
[14:08:55.161][I][lg-controller:2626]: SLAVE first matching indoor 0xC8; waiting for second consistent 0xC8
[14:08:55.161][D][lg-controller:2865]: ignoring incoming settings changes because of an outgoing pending change
[14:08:56.569][D][lg-controller:2422]: received C8.12.00.02.40.04.D1.1E.00.00.00.00.5A (13)
[14:08:56.574][I][lg-controller:2638]: SLAVE SLEEP CANCEL CONFIRMED: reservation cleared (Master A8 and two consistent indoor C8 status reports)
[14:08:56.574][I][lg-controller:2649]: SLAVE TX CONFIRMED by two indoor 0xC8 reports after 1 attempt(s) (47674 ms after TX)
```

### 6. Power-off synchronisation

The genuine LG system clears sleep reservations when the unit is
switched OFF.

V2 includes additional state handling to avoid retaining or retrying
obsolete reservations during power transitions.

It uses Master and indoor-unit reports to distinguish genuine state
changes from delayed or interleaved messages.

### 7. Avoiding competing shutdown timers

In Slave mode, the ESP32 should not independently power off the indoor
unit when its locally displayed sleep countdown expires.

The physical LG Master remains responsible for the actual reservation
and expiry behaviour.

The ESP32 requests, observes and synchronises the reservation instead
of operating a separate shutdown mechanism.

### 8. Preserving reservations during other operations

Outgoing Slave messages are constructed more carefully to avoid
unintentionally changing existing reservations when issuing unrelated
commands such as temperature or fan-speed adjustments.

Additional diagnostic retrieval has also been tested while sleep
reservations are active on standard indoor units.

Floor Console behaviour remains more conservatively configured pending
additional model-specific testing.

## Eight-hour reservation support

Most wired controllers tested in this installation support sleep
reservations of up to 8 hours (480 minutes).

The enhanced V2 component exposes a Home Assistant reservation range
of 0–480 minutes, extending the previous 420-minute limit.

The 420-minute reservation and Master countdown have been observed
successfully.

However, complete end-to-end verification of a 480-minute reservation
initiated through the ESP32, including expiry, remains outstanding.

## Testing and remaining limitations

The V2 regression tests cover reservation encoding and decoding,
Master confirmation, Slave synchronisation, power-state handling,
cancellation and external-change detection.

Real hardware testing has demonstrated several of these improvements,
but further validation remains worthwhile for:

- Reservations changed directly at the physical wall controller.
- Cancellation across different indoor models and operating modes.
- Mode changes and power transitions during active reservations.
- Reservation expiry and ESP32 reboot behaviour.
- Full 480-minute reservation operation.
- Floor Console-specific behaviour.

## Summary

The principal V2 improvement is not discovering that LG transmits
the sleep countdown; this was already documented by JanM321.

It is making better use of that information to keep Home Assistant
synchronised with the genuine LG Master, confirm requested changes,
handle external changes and avoid competing timer behaviour.
