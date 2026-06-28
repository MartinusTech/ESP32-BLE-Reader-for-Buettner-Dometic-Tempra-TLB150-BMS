/*
  myToken.h
  ------------------------------------------------------------
  Personal configuration - battery-specific values.

  This file defines TARGET_MAC and AEN_TOKEN, which are unique to
  YOUR OWN battery unit (a paired Bluetooth password and a hardware
  address). Keeping them here, separate from the main .ino file,
  means the .ino can be published/shared without accidentally
  publishing your own values.

  HOW TO FILL THIS IN:
  - AEN_TOKEN: capture it once from the official app using
    Wireshark + an Android phone's Bluetooth HCI snoop log.
    See docs/find-aen-token.md for the step-by-step guide.
  - TARGET_MAC: your battery's Bluetooth MAC address. Optional -
    the sketch also matches by device name ("TLB150"), so it will
    usually find your battery either way. Only useful if you have
    multiple TLB150 units nearby and want to target a specific one.

  IMPORTANT: Do not commit your real values back into a
  shared/public repository. Consider adding this file to
  .gitignore once you have filled in your own values.
  ------------------------------------------------------------
*/

#pragma once

static const char* TARGET_MAC = "AA:BB:CC:DD:EE:FF";          // <-- replace with YOUR battery's MAC address
static const char* AEN_TOKEN = "REPLACE_WITH_YOUR_OWN_TOKEN"; // <-- replace with YOUR captured token
