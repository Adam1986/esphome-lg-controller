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
