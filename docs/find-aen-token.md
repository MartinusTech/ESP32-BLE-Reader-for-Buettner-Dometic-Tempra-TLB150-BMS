# How to Find the `APP+AEN=` Token with Wireshark (Android BLE Capture)

A short, repeatable procedure for extracting the current `APP+AEN=` login
token sent by the Dometic/Büttner app to the Tempra TLB150 battery, in case
it ever changes.

---

## Prerequisites (one-time setup, assumed already in place)

- Android phone with **Developer Options → Bluetooth HCI snoop log** enabled
- Phone connected to the PC via **USB**, with the snoop log actively capturing
- Wireshark installed on the PC, reading the live HCI snoop stream from the phone
- adb installed on your PC (see Android Developer Platform Tools)
- The Dometic/Büttner app installed and working on the phone

---

## Step-by-step procedure

### 1. Start the capture

execute ".\adb devices" to enable the snoop log
Open Wireshark on the PC and confirm it is receiving live traffic from the
phone's Bluetooth HCI snoop interface (you should already see packets
scrolling as Bluetooth activity happens on the phone).

### 2. Open the app and connect

On the phone, open the Dometic/Büttner app and let it connect to the
battery as normal. Wait until the app shows live data (this confirms the
full handshake, including the login, has completed).

### 3. Apply the display filter

In Wireshark's display filter bar, enter:

```
frame contains "APP+AEN="
```

Press **Enter**.

This searches the raw bytes of every captured frame for that literal ASCII
string, regardless of which BLE handle, characteristic, or opcode carries
it — so it still works even if internal details change, as long as the
command prefix itself stays the same.

### 4. Open the matching packet

Wireshark should now show one (or a small number of) matching packets.
Click on the first one in the list.

### 5. Read the token

In the bottom **"Packet Bytes"** pane, look at the right-hand ASCII column.
You will see the full string, e.g.:

```
APP+AEN=YourCurrentToken
```

Everything after the `=` sign is the current token. Copy it exactly
(case-sensitive).

### 6. Update `myToken.h`

Paste the new value into the `AEN_TOKEN` constant inside `myToken.h`
(located in the same folder as the `.ino` file). You do not need to
touch the `.ino` file itself:

```
static const char* AEN_TOKEN = "REPLACE_WITH_YOUR_OWN_TOKEN";
```

---

## If the simple filter finds nothing

Try narrowing it down with a more specific combined filter (replace the MAC
address with your battery's actual address):

```
bluetooth.addr == AA:BB:CC:DD:EE:FF && btatt.opcode == 0x52 && frame contains "APP+AEN="
```



- `bluetooth.addr` restricts to packets involving the battery's MAC address
- `btatt.opcode == 0x52` restricts to ATT **Write Command** packets (the
  "Write No Response" type used by the app's main command channel)

If that still finds nothing, double-check:
- The snoop log was actually capturing *during* the connection attempt
  (toggling Bluetooth off/on in Developer Options sometimes resets the
  snoop log file)
- The app was closed and freshly reopened so it performs the full
  connect-and-login sequence (an already-connected app in the background
  won't repeat the login)

---

## Quick reference

| What | Value |
|---|---|
| Filter | `frame contains "APP+AEN="` |
| Where to look | Packet Bytes pane → ASCII column |
| What to copy | Everything after `APP+AEN=` |
| Where to paste it | `AEN_TOKEN` constant in `myToken.h` (not the `.ino` file) |
