/*
  TLB150_BLE_LiveData.ino
  ------------------------------------------------------------
  Copyright (c) 2026 [your name/handle here]
  Licensed under the Apache License, Version 2.0 - see LICENSE file in this repo.

  DISCLAIMER: This is an independent, community reverse-engineering
  project. It is NOT affiliated with, endorsed by, or supported by
  Dometic, Buettner Elektronik, or Telit. All product and company
  names are trademarks of their respective owners. Provided "AS IS",
  without warranty of any kind - use at your own risk. The author
  is not responsible for any damage to hardware, batteries,
  vehicles, or other systems. This project exists solely to let
  owners of this battery read their own telemetry data without
  depending on the official app (interoperability), and does not
  distribute any proprietary code or assets from the original app.

  See README.md for setup instructions, credits, and full details.
  ------------------------------------------------------------

  Reads live data from the Dometic/Buettner Tempra TLB150 battery
  over BLE - based on a protocol reverse-engineered from a genuine
  app capture (Wireshark/btsnoop).

  Reverse engineered for interoperability and based on observed data.

  HARDWARE: ESP32 (Arduino-ESP32 core, any version)
  LIBRARY: NimBLE-Arduino (h2zero), version 2.x

  ============================================================
  REVERSE-ENGINEERING STATUS (see below for details)
  ============================================================

  HARDWARE/PROTOCOL FUNDAMENTALS (confirmed):
  - Tempra TLB150 with firmware 5.1
  - Radio module: Telit BlueMod+S50 (formerly Stollmann E+V GmbH),
    identified via the GAP device name "BM+S50 4C4" and service UUID
    0xFEFB (officially registered to Telit with the Bluetooth SIG).
  - Proprietary service 0xFEFB = Telit's "Terminal IO V2"
    (transparent UART bridge), base UUID
    0000xxxx-0000-1000-8000-008025000000.
  - 3 channels (each a Write+Notify/Indicate pair):
      Channel A: TX=...0001 (Write No Resp.) / RX=...0002 (Notify)
                 -> main command channel ("APP+xxx"/"MST+xxx")
      Channel B: TX=...0003 (Write) / RX=...0004 (Indicate)
      Channel C: TX=...0009 (Write) / RX=...000A (Indicate)
  - Handshake: Channel C delivers a value via Indicate (always
    0x9B00), which must be written back unchanged. Channel B
    delivers an Indicate (always 0x12); the reply to it is fixed
    at 0xFF. Later, during normal operation, the app periodically
    writes FF/DF alternately (keepalive, exact purpose uncertain).
  - Login: "APP+AEN=xxxxxxxxxxxxx" - this value was IDENTICAL in
    every capture so far (not a random token). It likely matches
    the battery's Bluetooth password/PIN (entered once by the user
    when pairing in the original app). CONFIRMED: the battery
    actively VALIDATES this value (it is not just a formality) - it
    must match EXACTLY the value used by the original app, or the
    login/connection will fail. If the battery's Bluetooth password
    is ever changed, the value must be re-captured via a fresh
    Wireshark capture and updated in AEN_TOKEN in myToken.h (see the
    separate how-to guide for this).
  - Command sequence: APP+AEN -> APP+NET -> APP+DAT -> APP+IMP
    (including an echo reply for entry B1) -> APP+RDN=1. After
    that, the battery streams telemetry on its own (no further
    polling needed, aside from the periodic APP+NET keepalive).
  - Telemetry frames: 8 bytes, format [0x23][0x85][0x0F][ParamID]
    [4 data bytes]. The order of ParamIDs is NOT fixed (the battery
    appears to send each register individually whenever its
    internal update rate causes it to change/refresh).

  CONFIRMED VALUES (verified multiple times against the app display)
  - ParamID 0x02: VOLTAGE + CURRENT, [Vh][Vl][Ih][Il], big-endian,
    V=raw*0.01V, I=(raw & 0x7FFF)*0.01A, bit 15 of I = 1 ->
    discharging, 0 -> charging.
  - ParamID 0x56/0x57 (cell voltages) visibly rose during a
    charging phase, from ~3.32V to ~3.40V/cell.
  - ParamID 0x0B: State of Charge (SoC) in % - confirmed
  - ParamID 0x0E: "Quality" in % - confirmed
  - ParamID 0x36: Energy in Wh, big-endian 16-bit - confirmed
    (rises/falls live with charging/discharging).
  - Nominal capacity 150 Ah: ParamID 0x07 and IMP table entry B3 -
    confirmed
  - ParamID 0x56 AND 0x57: presumably INDIVIDUAL CELL VOLTAGES
    (0x56=cell1+2, 0x57=cell3+4) of a 4S pack, big-endian 16-bit /
    1000 = volts.

  STILL UNRESOLVED:
  - PARAMETER 0x34: data fields H1 (non-0xFFFF only while charging)
    and H2 (non-0xFFFF only while discharging) - the field is not
    yet analyzed, meaning of the contained data is unknown (see
    NEXT STEPS below).
  - STATUS FIELD: no longer strictly necessary via 0x02's sign bit
    (charging/discharging) - that already covers the most important
    part ("am I charging or not"). Finer states (e.g.
    bulk/absorption/float while charging) remain separately open.
  - DAT stream (channel marker 0x4D, triggered by APP+DAT): values
    are byte-identical in every capture -> presumably fixed
    calibration data, not a live measurement. Still unclear exactly
    what it represents.

  NEXT STEPS (TODO):
  1. Determine the meaning of ParamID 0x34's data fields - H1
     (non-0xFFFF only while charging) and H2 (non-0xFFFF only while
     discharging). Field is not yet analyzed.
  2. Further clarify the DAT stream, low priority. Ideally with a
     capture during a longer, high-current charging session.

  IMPORTANT NOTE on AEN_TOKEN:
  This value must be taken 1:1 from a Wireshark capture of the real
  Dometic/Buettner app while it connects to YOUR OWN battery.
  CONFIRMED: the battery actively validates this value - it must
  match EXACTLY the value used by the original app, or the login
  will fail (it is not just a formality). If your battery's
  password is ever changed, the current value must be re-determined
  via a Wireshark/HCI-snoop capture of the original app and
  substituted in myToken.h (see below).

  NOTE FOR PUBLISHED/SHARED USE:
  TARGET_MAC and AEN_TOKEN are NOT defined in this .ino file. They
  are pulled in at compile time from a separate file, "myToken.h",
  located in the same sketch folder (see the #include below). Both
  values are specific to an individual battery unit (a paired
  password and a hardware address) - keeping them in their own file
  means this .ino can be published/shared without accidentally
  publishing your own values. Just fill in your own values in
  myToken.h and keep that file out of any shared/public repository
  (e.g. add it to .gitignore). See the README / the Wireshark
  how-to guide for how to obtain your own values.

  WHAT THIS SKETCH DOES:
   1. Scans for the battery (name contains "TLB150", or a fixed MAC)
   2. Connects, looks up the proprietary Telit service 0xFEFB with
      its 6 characteristics (3 channels: A/B/C)
   3. Performs the handshake (see above)
   4. Sends APP+AEN -> APP+NET -> APP+DAT -> APP+IMP (incl. echo)
      -> APP+RDN=1
   5. Prints all incoming data to the Serial Monitor, including
      timestamps [t=...s], plain-text MST replies, decoded
      telemetry values (see list above), and always the raw value
      as well
   6. Sends a keepalive every ~12s (APP+NET + channel B toggle)
   7. Computes a SMOOTHED estimated remaining runtime (days/hours/
      minutes, like the official app) from Wh (0x36) and power
      (from U*I of 0x02). The instantaneous power is smoothed with
      a time-based exponential moving average (time constant 30s,
      see POWER_SMOOTHING_TAU_S) before computing the runtime - this
      prevents the constant "jumping" between two values that was
      also observed in the app itself.
      While charging, only the smoothed charging power is shown
      instead of a runtime estimate (no time-to-full calculation,
      since we lack the necessary "Wh until full" reference).
   8. ROBUSTNESS: nothing gets permanently "stuck" anymore - a scan
      with no match is restarted automatically, and a failed or
      aborted connection attempt cleans up and scans again. If no
      connection succeeds for an extended period (default: 3
      minutes), the sketch performs a full restart (ESP.restart())
      as a safety net - in case the BLE stack ever ends up in a
      stuck state that would otherwise only be fixable by a manual
      power cycle.
  ------------------------------------------------------------
*/

#include <NimBLEDevice.h>
#include <math.h>

// ================= Configuration =================
static const char* TARGET_NAME_SUBSTR = "TLB150";
#include "myToken.h" // <-- defines TARGET_MAC and AEN_TOKEN, see myToken.h / README
static const uint32_t SCAN_SECONDS = 15;
static const unsigned long KEEPALIVE_INTERVAL_MS = 12000;
// ===================================================

// 128-bit characteristic UUIDs (Telit "Terminal IO V2" base UUID)
static NimBLEUUID SVC_UUID((uint16_t)0xFEFB);
static NimBLEUUID UUID_TX_A("00000001-0000-1000-8000-008025000000"); // Write No Response
static NimBLEUUID UUID_RX_A("00000002-0000-1000-8000-008025000000"); // Notify
static NimBLEUUID UUID_TX_B("00000003-0000-1000-8000-008025000000"); // Write
static NimBLEUUID UUID_RX_B("00000004-0000-1000-8000-008025000000"); // Indicate
static NimBLEUUID UUID_TX_C("00000009-0000-1000-8000-008025000000"); // Write
static NimBLEUUID UUID_RX_C("0000000a-0000-1000-8000-008025000000"); // Indicate

// ================= Global state =================
static bool doConnect = false;
static NimBLEAddress targetAddress;
static NimBLEClient* pClient = nullptr;

static NimBLERemoteCharacteristic *pTxA = nullptr, *pRxA = nullptr;
static NimBLERemoteCharacteristic *pTxB = nullptr, *pRxB = nullptr;
static NimBLERemoteCharacteristic *pTxC = nullptr, *pRxC = nullptr;

volatile bool gotIndicateB = false;
volatile bool gotIndicateC = false;
uint8_t indicateBValue[8]; size_t indicateBLen = 0;
uint8_t indicateCValue[8]; size_t indicateCLen = 0;

volatile bool gotImpB1 = false;
uint8_t impB1Bytes[4];

unsigned long lastKeepalive = 0;
bool toggleState = true;
bool isReady = false;

// ---- Remaining runtime estimate (smoothed) ----
// Instantaneous U/I values (0x02) fluctuate from one second to the
// next (e.g. 1.18-1.35A), which makes a runtime estimate computed
// directly from them "jump around" noticeably. We therefore smooth
// the power with a time-based exponential moving average (EMA)
// before using it to compute the remaining runtime.
static float smoothedPowerW = 0;          // positive=charging, negative=discharging
static bool havePowerSample = false;
static unsigned long lastPowerSampleMs = 0;
static const float POWER_SMOOTHING_TAU_S = 30.0; // smoothing time constant, in seconds
static uint16_t lastWh = 0;
static bool haveWh = false;

// ---- Connection robustness ----
static bool scanActive = false;
static unsigned long scanStartedAt = 0;
static unsigned long lastSuccessTime = 0;
static int consecutiveFailures = 0;
// After this much time WITHOUT a successful connection: perform a
// full ESP32 restart as a last resort (e.g. in case the BLE stack
// ends up in a stuck state that can only be fixed by a restart).
static const unsigned long RESTART_WATCHDOG_MS = 3UL * 60UL * 1000UL; // 3 minutes

// ================= Helper functions =================
void printHex(const uint8_t* d, size_t n) {
  for (size_t i = 0; i < n; i++) Serial.printf("%02X ", d[i]);
}

// Formats an hour count as "XdYhZm", similar to the app's display.
void formatDuration(float hours, char* outBuf, size_t outBufLen) {
  if (hours < 0) hours = 0;
  unsigned long totalMinutes = (unsigned long)(hours * 60.0 + 0.5);
  unsigned long d = totalMinutes / (24 * 60);
  unsigned long h = (totalMinutes / 60) % 24;
  unsigned long m = totalMinutes % 60;
  snprintf(outBuf, outBufLen, "%lud %luh %lum", d, h, m);
}

void handleParamFrame85(const uint8_t* f) {
  // f[0]=0x23 f[1]=0x85 f[2]=0x0F f[3]=ParamID f[4..7]=4 data bytes
  uint8_t id = f[3];
  Serial.printf("[t=%lus]     [RDN] ID=0x%02X data=", millis() / 1000, id);
  printHex(f + 4, 4);
  if (id == 0x0B)       Serial.printf(" -> SoC=%u%%", f[4]);
  else if (id == 0x0E)  Serial.printf(" -> Quality=%u%%", f[4]);
  else if (id == 0x36) {
    uint16_t wh = (f[4] << 8) | f[5]; // big-endian
    Serial.printf(" -> Wh=%u", wh);
    lastWh = wh;
    haveWh = true;
  }
  else if (id == 0x02) {
    // Register 0x85.02:
    // [Vh][Vl][Ih][Il], V=0.01V (big-endian), I=0.01A (big-endian),
    // bit 15 of I = 1 -> discharging, 0 -> charging
    uint16_t vRaw = (f[4] << 8) | f[5];
    uint16_t iRaw = (f[6] << 8) | f[7];
    float V = vRaw * 0.01;
    bool discharging = iRaw & 0x8000;
    float I = (iRaw & 0x7FFF) * 0.01;
    Serial.printf(" -> U=%.2fV I=%.2fA (%s)", V, I, discharging ? "discharging" : "charging");

    // ---- Remaining runtime (smoothed) ----
    // Signed instantaneous power: negative=discharging, positive=charging
    float instPowerW = V * I * (discharging ? -1.0 : 1.0);
    unsigned long now = millis();
    if (!havePowerSample) {
      smoothedPowerW = instPowerW;
      havePowerSample = true;
    } else {
      float dtS = (now - lastPowerSampleMs) / 1000.0;
      if (dtS > 0) {
        // Time-based EMA smoothing factor: the longer since the
        // last sample, the more strongly the new value pulls
        float alpha = 1.0 - exp(-dtS / POWER_SMOOTHING_TAU_S);
        smoothedPowerW += alpha * (instPowerW - smoothedPowerW);
      }
    }
    lastPowerSampleMs = now;

    if (haveWh) {
      if (smoothedPowerW < -0.5) {
        // Net discharging: remaining time until empty
        float hoursLeft = lastWh / (-smoothedPowerW);
        char buf[24];
        formatDuration(hoursLeft, buf, sizeof(buf));
        Serial.printf(" | Remaining runtime (smoothed, P=%.1fW)=%s", smoothedPowerW, buf);
      } else if (smoothedPowerW > 0.5) {
        Serial.printf(" | Charging (smoothed P=%.1fW), no runtime estimate", smoothedPowerW);
      } else {
        Serial.print(" | near equilibrium, no runtime estimate");
      }
    }
  }
  else if (id == 0x34) {
    // Field H1 is non-0xFFFF only while charging, H2 is non-0xFFFF
    // only while discharging. The meaning of the contained data is
    // not yet determined - see "NEXT STEPS" above. Printed as raw
    // hex only, no interpretation.
    uint16_t h1 = (f[4] << 8) | f[5];
    uint16_t h2 = (f[6] << 8) | f[7];
    if (h1 != 0xFFFF) {
      Serial.printf(" -> 0x34-H1(charging, meaning unknown)=0x%04X", h1);
    }
    if (h2 != 0xFFFF) {
      Serial.printf(" -> 0x34-H2(discharging, meaning unknown)=0x%04X", h2);
    }
  }
  else if (id == 0x56 || id == 0x57) {
    // Individual cell voltages (0x56=cell1+2, 0x57=cell3+4),
    // big-endian 16-bit / 1000 = volts (4S pack, ~3.2-3.65V/cell expected)
    uint16_t c1 = (f[4] << 8) | f[5];
    uint16_t c2 = (f[6] << 8) | f[7];
    Serial.printf(" -> Cell voltage %.3fV / %.3fV", c1 / 1000.0, c2 / 1000.0);
  }
  else if (id == 0xF2) {
    Serial.printf(" -> F2(?)=0x%02X", f[5]);
  }
  Serial.println();
}

void handleCellFrame4D(const uint8_t* f) {
  // f[0]=0x23 f[1]=0x4D f[2]=channel/index f[3]=0xB1(marker) f[4..7]=2x16-bit (LE)
  uint8_t idx = f[2];
  uint16_t v1 = f[4] | (f[5] << 8);
  uint16_t v2 = f[6] | (f[7] << 8);
  Serial.printf("    [DAT] Channel=%u  Value1=%u (=%.3f ?)  Value2=%u\n", idx, v1, v1 / 1000.0, v2);
}

// Main channel A: notify callback (text replies + binary telemetry frames)
void notifyCallbackA(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t length, bool isNotify) {
  bool looksAscii = (length >= 4 && data[0] == 'M' && data[1] == 'S' && data[2] == 'T' && data[3] == '+');
  if (looksAscii) {
    String s;
    for (size_t i = 0; i < length; i++) s += (char)data[i];
    Serial.printf("[t=%lus]   [MST] %s\n", millis() / 1000, s.c_str());
    if (s.startsWith("MST+IMP=") && length >= 13 && data[8] == 0xB1) {
      memcpy(impB1Bytes, data + 9, 4);
      gotImpB1 = true;
    }
    return;
  }

  // Binary frames: parsed in 8-byte steps starting at each 0x23 marker
  size_t i = 0;
  bool parsedAny = false;
  while (i + 8 <= length) {
    if (data[i] == 0x23) {
      if (data[i + 1] == 0x85 && data[i + 2] == 0x0F) {
        handleParamFrame85(data + i);
        i += 8; parsedAny = true; continue;
      } else if (data[i + 1] == 0x4D) {
        handleCellFrame4D(data + i);
        i += 8; parsedAny = true; continue;
      }
    }
    i++;
  }
  Serial.print("  [RAW] ");
  printHex(data, length);
  Serial.println();
  (void)parsedAny;
}

void indicateCallbackB(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t length, bool isNotify) {
  Serial.print("  [INDICATE Channel B] "); printHex(data, length); Serial.println();
  if (!gotIndicateB) {
    indicateBLen = length;
    memcpy(indicateBValue, data, length);
    gotIndicateB = true;
  }
}

void indicateCallbackC(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t length, bool isNotify) {
  Serial.print("  [INDICATE Channel C] "); printHex(data, length); Serial.println();
  if (!gotIndicateC) {
    indicateCLen = length;
    memcpy(indicateCValue, data, length);
    gotIndicateC = true;
  }
}

class MyScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    String name = advertisedDevice->haveName() ? advertisedDevice->getName().c_str() : "";
    String addr = advertisedDevice->getAddress().toString().c_str();
    if (name.indexOf(TARGET_NAME_SUBSTR) >= 0 || addr.equalsIgnoreCase(TARGET_MAC)) {
      Serial.printf("Found: %s (%s)\n", name.c_str(), addr.c_str());
      targetAddress = advertisedDevice->getAddress();
      NimBLEDevice::getScan()->stop();
      scanActive = false;
      doConnect = true;
    }
  }
  // Note: deliberately NO onScanEnd() override - its signature has
  // changed multiple times between NimBLE-Arduino versions. Instead,
  // loop() detects via a time comparison (scanStartedAt + SCAN_SECONDS)
  // whether a scan has timed out without a match, and restarts it itself.
};

void startScan() {
  Serial.printf("\n[t=%lus] Starting scan (%lu seconds)...\n",
                millis() / 1000, (unsigned long)SCAN_SECONDS);
  scanActive = true;
  scanStartedAt = millis();
  NimBLEDevice::getScan()->start(SCAN_SECONDS, false);
}

// Cleanly disconnects any still-open connection and releases the
// client, so the next connection attempt doesn't build on a stuck
// state.
void cleanupClient() {
  if (pClient != nullptr) {
    if (pClient->isConnected()) {
      pClient->disconnect();
      delay(200);
    }
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }
  pTxA = pRxA = pTxB = pRxB = pTxC = pRxC = nullptr;
  gotIndicateB = gotIndicateC = gotImpB1 = false;
}

bool waitFor(volatile bool &flag, unsigned long timeoutMs) {
  unsigned long start = millis();
  while (!flag && millis() - start < timeoutMs) delay(20);
  return flag;
}

void sendCommand(const char* cmd) {
  Serial.printf("[t=%lus] >> Sending: %s\n", millis() / 1000, cmd);
  pTxA->writeValue((uint8_t*)cmd, strlen(cmd), false); // Write No Response
}

bool setupBattery() {
  Serial.printf("Connecting to %s ...\n", targetAddress.toString().c_str());
  pClient = NimBLEDevice::createClient();

  bool connected = false;
  for (int attempt = 1; attempt <= 3 && !connected; attempt++) {
    if (attempt > 1) {
      Serial.printf("  Connection attempt %d/3 ...\n", attempt);
      delay(800);
    }
    connected = pClient->connect(targetAddress);
  }

  if (!connected) {
    Serial.println("ERROR: connection failed after 3 attempts.");
    return false;
  }
  Serial.println("Connected. Looking up service 0xFEFB...");

  NimBLERemoteService* svc = pClient->getService(SVC_UUID);
  if (!svc) {
    Serial.println("ERROR: service 0xFEFB not found.");
    return false;
  }

  pTxA = svc->getCharacteristic(UUID_TX_A);
  pRxA = svc->getCharacteristic(UUID_RX_A);
  pTxB = svc->getCharacteristic(UUID_TX_B);
  pRxB = svc->getCharacteristic(UUID_RX_B);
  pTxC = svc->getCharacteristic(UUID_TX_C);
  pRxC = svc->getCharacteristic(UUID_RX_C);

  if (!pTxA || !pRxA || !pTxB || !pRxB || !pTxC || !pRxC) {
    Serial.println("ERROR: not all 6 characteristics were found.");
    return false;
  }
  Serial.println("All 6 characteristics found.");

  // --- Channel C: subscribe to Indicate, echo the value back ---
  Serial.println("\n[1/3] Setting up channel C (Indicate 0000000A)...");
  // Note: if the method is named differently on your version (e.g.
  // registerForNotify instead of subscribe), replace
  // subscribe(false, cb) with registerForNotify(cb, false)
  pRxC->subscribe(false, indicateCallbackC);
  if (!waitFor(gotIndicateC, 5000)) {
    Serial.println("ERROR: no Indicate received on channel C.");
    return false;
  }
  Serial.print("  -> Echoing back on channel C: ");
  printHex(indicateCValue, indicateCLen);
  Serial.println();
  pTxC->writeValue(indicateCValue, indicateCLen, true);

  // --- Channel B: subscribe to Indicate, write a fixed value ---
  Serial.println("\n[2/3] Setting up channel B (Indicate 00000004)...");
  pRxB->subscribe(false, indicateCallbackB);
  if (!waitFor(gotIndicateB, 5000)) {
    Serial.println("ERROR: no Indicate received on channel B.");
    return false;
  }
  uint8_t ffByte = 0xFF;
  Serial.println("  -> Writing 0xFF to channel B");
  pTxB->writeValue(&ffByte, 1, true);

  // --- Channel A: main communication ---
  Serial.println("\n[3/3] Activating main channel A (Notify 00000002)...");
  pRxA->subscribe(true, notifyCallbackA);
  delay(300);

  Serial.println("\n--- Login & data request ---");
  String aenCmd = String("APP+AEN=") + AEN_TOKEN;
  sendCommand(aenCmd.c_str());
  delay(500);
  sendCommand("APP+NET");
  delay(500);
  sendCommand("APP+DAT");
  delay(1000);
  sendCommand("APP+IMP");
  delay(800);

  if (waitFor(gotImpB1, 3000)) {
    uint8_t buf[13] = {'A','P','P','+','I','M','P','=', 0xB1,
                        impB1Bytes[0], impB1Bytes[1], impB1Bytes[2], impB1Bytes[3]};
    Serial.print(">> Sending APP+IMP= echo: ");
    printHex(buf, sizeof(buf));
    Serial.println();
    pTxA->writeValue(buf, sizeof(buf), false);
  } else {
    Serial.println("No IMP B1 entry received, skipping echo write.");
  }
  delay(500);

  sendCommand("APP+RDN=1");

  lastKeepalive = millis();
  toggleState = true;

  Serial.println("\n=========================================================");
  Serial.println(" Setup complete - live data should now be arriving");
  Serial.println("=========================================================\n");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("====================================================");
  Serial.println(" Buettner/Dometic Tempra TLB150 - live data reader");
  Serial.println("====================================================");

  NimBLEDevice::init("ESP32-BMS-Reader");
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(new MyScanCallbacks());
  pScan->setActiveScan(true);

  lastSuccessTime = millis(); // reset the watchdog clock at startup
  startScan();
}

void loop() {
  unsigned long now = millis();

  // ---- 1) Handle the result of an in-progress connection attempt ----
  if (doConnect) {
    doConnect = false;
    isReady = setupBattery();

    if (isReady) {
      consecutiveFailures = 0;
      lastSuccessTime = now;
    } else {
      consecutiveFailures++;
      Serial.printf("[t=%lus] Connection attempt failed (failure #%d in a row). "
                     "Cleaning up and retrying...\n",
                     now / 1000, consecutiveFailures);
      cleanupClient();
      delay(500);
      startScan();
    }
  }

  // ---- 2) Monitor an active connection (keepalive / connection loss) ----
  if (isReady && pClient != nullptr) {
    if (pClient->isConnected()) {
      lastSuccessTime = now; // as long as we're connected, this counts as "success" for the watchdog
      if (now - lastKeepalive > KEEPALIVE_INTERVAL_MS) {
        lastKeepalive = now;
        sendCommand("APP+NET");
        uint8_t toggleByte = toggleState ? 0xFF : 0xDF;
        toggleState = !toggleState;
        pTxB->writeValue(&toggleByte, 1, true);
      }
    } else {
      Serial.printf("[t=%lus] Connection lost. Cleaning up and starting a new scan...\n", now / 1000);
      isReady = false;
      cleanupClient();
      startScan();
    }
  }

  // ---- 3) Detect scan timeout (no match within SCAN_SECONDS) and restart ----
  if (scanActive && !doConnect && (now - scanStartedAt) > (SCAN_SECONDS * 1000UL + 1000UL)) {
    Serial.printf("[t=%lus] Scan timed out without a match. Restarting...\n", now / 1000);
    scanActive = false;
    startScan();
  }

  // If, for whatever reason, we're neither scanning nor connected nor
  // have a connection attempt in progress (shouldn't normally happen),
  // kick off a scan again just to be safe.
  if (!isReady && !doConnect && !scanActive) {
    startScan();
  }

  // ---- 4) Watchdog: too long without success -> full restart ----
  if (!isReady && (now - lastSuccessTime) > RESTART_WATCHDOG_MS) {
    Serial.printf("[t=%lus] Watchdog: no successful connection for %lu seconds. "
                   "Performing a full restart...\n",
                   now / 1000, (now - lastSuccessTime) / 1000);
    delay(1000);
    ESP.restart();
  }

  delay(100);
}
