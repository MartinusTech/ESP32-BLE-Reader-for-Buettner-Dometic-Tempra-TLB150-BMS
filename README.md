# TLB150 BLE Live Data Reader

An ESP32/Arduino sketch that reads live telemetry (voltage, current,
state of charge, cell voltages, energy, estimated remaining
runtime, ...) from a **Dometic / Büttner Tempra TLB150** lithium
battery over Bluetooth Low Energy — without needing the official
app.

Currently, the data is output via the serial console. Extending this
towards MQTT or other transmission systems could easily be implemented.

The communication protocol was reverse-engineered from a genuine
Bluetooth HCI snoop capture of the official app, by observing and
decoding its behavior.

---

## Disclaimer

This is an independent, community reverse-engineering project.
**It is not affiliated with, endorsed by, or supported by Dometic,
Büttner Elektronik, or Telit.** All product and company names are
trademarks of their respective owners.

This project exists purely for **interoperability**: to let owners
of this battery read their own telemetry data without depending on
the official app. No proprietary code, binaries, or assets from the
original app are included or distributed here — everything in this
repository is original code based on observed protocol behavior.

This software is provided **"AS IS", without warranty of any kind**.
It is **read-only** — it does not send any command that changes
battery settings or behavior — but you are interacting with the
BLE interface of a lithium battery system, so use it at your own
risk. The author is not responsible for any damage to hardware,
batteries, vehicles, or other systems. See [LICENSE](LICENSE) for
the full license text.

---

## What this sketch does

- Scans for and connects to the battery over BLE
- Performs the (reverse-engineered) login/handshake sequence
- Streams and decodes live telemetry, including:
  - Voltage and current (with charge/discharge direction)
  - State of charge (SoC) and "quality"
  - Energy (Wh)
  - Smoothed estimated remaining runtime (days/hours/minutes)
  - Individual cell voltages
  - Nominal capacity (Ah)
- Automatically recovers from failed scans/connections, and performs
  a full restart as a last-resort safety net if nothing succeeds for
  an extended period

See the comment block at the top of the `.ino` file for the full,
detailed protocol write-up, including which values are confirmed and
which are still unconfirmed/open.

## Status / known limitations

Several telemetry fields are still not fully understood (most
notably parameter `0x34`'s two data fields, and a static
calibration-looking data stream). See the in-code documentation for
details and current best guesses. Contributions/findings welcome.

---

## Requirements

- An ESP32 board (any variant should work)
- Arduino-ESP32 core (any reasonably recent version)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) library (h2zero), version 2.x

## Setup

1. **Get your battery's login token.** The battery validates a
   per-device (or per-pairing) token sent during login
   (`APP+AEN=...`). You need to capture this once from the official
   app using Wireshark + an Android phone's Bluetooth HCI snoop log.
   See [`docs/find-aen-token.md`](docs/find-aen-token.md) for a
   step-by-step guide.
2. **Fill in your personal config file.** Battery-specific values
   (`TARGET_MAC` and `AEN_TOKEN`) live in their own file,
   `myToken.h`, in the same folder as the `.ino` file - not in the
   `.ino` file itself. Open `myToken.h` and replace the placeholders:
   ```cpp
   static const char* TARGET_MAC = "AA:BB:CC:DD:EE:FF";          // your battery's MAC address
   static const char* AEN_TOKEN = "REPLACE_WITH_YOUR_OWN_TOKEN"; // your captured token
   ```
   (`TARGET_MAC` is optional — the sketch also matches by device
   name, so it will usually find your battery either way. It's only
   useful if you have multiple TLB150 units nearby and want to
   target a specific one.)

   The `.ino` file simply does `#include "myToken.h"` to pull these
   in at compile time, so you never need to edit the `.ino` file for
   this.
3. Flash the sketch to your ESP32 and open the Serial Monitor at
   115200 baud.

**Never commit your own real `myToken.h` (with your real token and/or
MAC address) back into a shared/public repository** - consider
adding it to `.gitignore`.

---

## Credits

- The voltage/current decoding formula for parameter `0x02` was
  cross-checked against an independent reverse-engineering project
  for the same battery family over its wired N-Bus interface:
  [Wannesgarrevoet/nbus-esp32](https://github.com/Wannesgarrevoet/nbus-esp32).
  Many thanks for the cross-reference.

## Contributing

Found a new parameter, a more accurate formula, or a bug? Issues and
pull requests are welcome.

## License

Apache License 2.0 — see [LICENSE](LICENSE).
