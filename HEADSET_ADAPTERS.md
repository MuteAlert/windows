# Headset mute providers

Headset mute synchronization intentionally separates button events from known
latched mute state. This avoids claiming that a quiet microphone is physically
muted.

## Built-in providers

Providers are selected in this order:

1. **SteelSeries device state** — the Arctis Nova Pro Wireless adapter reads a
   vendor status report and supplies a high-confidence latched state.
2. **Standard HID mute button** — Raw Input observes USB HID System Microphone
   Mute (`Generic Desktop 0x01 / 0xA9`), Phone Mute
   (`Telephony 0x0B / 0x2F`), and Call Mute Toggle
   (`Telephony 0x0B / 0xE1`). These are medium-confidence button events, not a
   guaranteed physical switch position.
3. **Windows hardware mute** — Core Audio reports that the active capture
   endpoint implements `ENDPOINT_HARDWARE_SUPPORT_MUTE`, and its endpoint mute
   state is used as a high-confidence driver-reported state.
4. **Unsupported/no observable state** — no trustworthy hardware signal is
   available. Audio silence is never used as a substitute.

After a standard HID event, the application briefly waits for Windows or the
active call application to react. It then synchronizes only layers whose mute
state did not change, reducing duplicate toggles.

## Adding a vendor protocol

Vendor adapters implement `IHeadsetMuteAdapter` in
`src/headset_adapters.h`. An adapter must:

- match only confirmed vendor and product identifiers;
- open HID interfaces with shared access;
- validate usage pages and report sizes before reading or writing;
- report `stateKnown = true` only when the device supplies an explicit mute
  state;
- never derive mute from a zero peak level;
- avoid sending undocumented write commands during discovery;
- close handles promptly after removal or failed I/O.

Add the adapter factory to `src/headset_adapters.cpp` and mark its entry in
`GetVendorAdapterSlots()` as implemented. Slots are already documented for
Logitech (`046D`), Jabra (`0B0E`), Poly/Plantronics (`047F`), and Corsair
(`1B1C`). These slots describe extension points, not implemented proprietary
protocols.

## Diagnostics and privacy

The Headset Settings page can export a text report containing:

- USB vendor and product IDs;
- manufacturer and product names when exposed by the device;
- top-level usage page and usage;
- input, output, and feature report lengths;
- recognized standard mute usages;
- registered vendor-adapter status;
- a rolling report-change log containing only report number, length, and
  changed byte offsets.

The export deliberately excludes HID device paths, serial numbers, and raw
report values. Contributors should ask users to reproduce one control change
at a time before exporting so changed offsets can be correlated safely.
