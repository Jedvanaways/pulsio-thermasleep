/*
 * NRF24L01+ as LT8910 sniffer + replayer
 *
 * Uses the NRF24L01+ modules from the original plan to talk to the
 * ThermaSleep pod's LT8910. This works because both chips are
 * 2.4GHz GFSK at 1 Mbps with compatible deviation (~160 vs 175 kHz).
 *
 * The NRF24's "address" field is used to match the LT8910 sync word,
 * and LT8910's 4-byte 0x55 preamble is long enough that NRF24 locks
 * onto it naturally.
 *
 * Well-proven in the drone-hacking community (DIY-Multiprotocol-TX-Module
 * uses this exact trick to control Cheerson CX-10 drones from NRF24
 * hardware).
 *
 * Hardware:
 *   - ESP32 dev kit (Binghe ESP-WROOM-32D or similar)
 *   - Standard NRF24L01+ module on SPI bus
 *
 * Wiring (ESP32 <-> NRF24L01+):
 *   NRF24 pin    ESP32 GPIO
 *   ----------   ----------
 *   VCC          3V3 (add 10uF cap at the module's VCC pin!)
 *   GND          GND
 *   CE           GPIO 4
 *   CSN          GPIO 5
 *   SCK          GPIO 18
 *   MOSI         GPIO 23
 *   MISO         GPIO 19
 *   IRQ          (not connected)
 *
 * Library: install "RF24 by TMRh20" from the Arduino Library Manager.
 *
 * Usage — open Serial Monitor @ 115200, commands:
 *   s              - RSSI scan all 84 channels (find the pod's channel)
 *   m <ch>         - promiscuous RX on channel ch for 30s (capture raw)
 *   r <ch>         - sync-word RX on channel ch for 30s (after sync known)
 *   p / u / d      - transmit Power / Up / Down payload
 *   c <ch>         - set TX channel
 *   ?              - help
 */

#include <SPI.h>
#include <RF24.h>
#include <Arduino.h>

// ---- Fill in after Phase 2 decode ----------------------------------
// (The defaults here are placeholders. Replace with values from decode.md.)

static uint8_t  RF_CHANNEL = 0;              // <<FILL>> 0-83 (2400 + n MHz)
static uint8_t  SYNC_WORD[5] = {             // <<FILL>> LT8910 sync word, MSB first
  0x00, 0x00, 0x00, 0x00, 0x00
};
static uint8_t  SYNC_LEN = 4;                // <<FILL>> 3, 4, or 5 bytes

// Captured payloads (without CRC — NRF24 CRC is disabled; LT8910 CRC is
// either disabled on pod side or we include it as extra payload bytes)
static const uint8_t POWER_PAYLOAD[] = {0x00};   // <<FILL>>
static const uint8_t UP_PAYLOAD[]    = {0x00};   // <<FILL>>
static const uint8_t DOWN_PAYLOAD[]  = {0x00};   // <<FILL>>
// --------------------------------------------------------------------

// Promiscuous-mode fake address. Matches the LT8910 preamble (0x55 bytes)
// + hopefully the first byte of the real sync word. This is the Bastille /
// Goodspeed trick for blind NRF24 capture — bytes after this "fake address"
// are the real sync word + payload that we then analyse.
static const uint8_t PROMISC_ADDR[3] = {0x55, 0x55, 0x55};

static const uint8_t CE_PIN  = 4;
static const uint8_t CSN_PIN = 5;

RF24 radio(CE_PIN, CSN_PIN);

// --- Helpers --------------------------------------------------------

static void printHex(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (p[i] < 0x10) Serial.print('0');
    Serial.print(p[i], HEX);
    Serial.print(' ');
  }
}

// NRF24 transmits address bytes LSB-first over the air. The RF24 library
// handles the endianness internally (library expects the address "as you
// want it on the wire", MSB first in the array). If sniffing shows nothing,
// try reversing the bytes.
static void setupRxWithAddress(uint8_t ch, const uint8_t* addr, uint8_t width) {
  radio.stopListening();
  radio.setChannel(ch);
  radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setAddressWidth(width);
  radio.setPayloadSize(32);
  radio.disableCRC();
  radio.openReadingPipe(1, (uint8_t*)addr);
  radio.startListening();
}

static void setupTx() {
  radio.stopListening();
  radio.setChannel(RF_CHANNEL);
  radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setAddressWidth(SYNC_LEN);
  radio.disableCRC();
  radio.setPALevel(RF24_PA_HIGH);
  radio.openWritingPipe(SYNC_WORD);
}

// --- Commands -------------------------------------------------------

static void rssiScanAll() {
  Serial.println(F("RSSI scan — 100 passes across 84 channels (~20s)."));
  Serial.println(F("Press ThermaSleep buttons repeatedly during the scan."));

  int counts[84] = {0};

  for (int pass = 0; pass < 100; pass++) {
    for (int ch = 0; ch < 84; ch++) {
      radio.setChannel(ch);
      radio.startListening();
      delayMicroseconds(200);
      if (radio.testRPD()) counts[ch]++;
      radio.stopListening();
    }
  }

  Serial.println(F("Done. Channels with >= 10 hits:"));
  int best = 0, bestCh = -1;
  for (int ch = 0; ch < 84; ch++) {
    if (counts[ch] >= 10) {
      Serial.print(F("  CH "));
      Serial.print(ch);
      Serial.print(F(" ("));
      Serial.print(2400 + ch);
      Serial.print(F(" MHz): "));
      Serial.print(counts[ch]);
      Serial.println(F(" hits"));
    }
    if (counts[ch] > best) { best = counts[ch]; bestCh = ch; }
  }
  if (bestCh >= 0) {
    Serial.print(F("Strongest: CH "));
    Serial.print(bestCh);
    Serial.print(F(" with "));
    Serial.print(best);
    Serial.println(F(" hits."));
  }
}

static void promiscuousOnChannel(uint8_t ch) {
  Serial.print(F("Promiscuous mode on CH "));
  Serial.println(ch);
  Serial.println(F("Logging any RX for 30 seconds. Press remote buttons now."));
  Serial.println(F("Bytes 0-31 = what NRF24 captured *after* the fake sync."));
  Serial.println(F("Look for repeated prefixes across captures — that's the real sync word."));

  setupRxWithAddress(ch, PROMISC_ADDR, 3);

  uint32_t start = millis();
  uint8_t buf[32];
  int count = 0;

  while (millis() - start < 30000) {
    if (radio.available()) {
      radio.read(buf, 32);
      Serial.print(F("RX["));
      if (count < 10) Serial.print(' ');
      Serial.print(count++);
      Serial.print(F("]: "));
      printHex(buf, 32);
      Serial.println();
    }
  }

  radio.stopListening();
  Serial.print(F("Captured "));
  Serial.print(count);
  Serial.println(F(" frames."));
  Serial.println(F("Analysis: first N bytes of each frame that are stable across"));
  Serial.println(F("all captures = real LT8910 sync word. Copy into SYNC_WORD[]."));
}

static void listenChannel(uint8_t ch) {
  Serial.print(F("Listening on CH "));
  Serial.print(ch);
  Serial.print(F(" with sync word "));
  printHex(SYNC_WORD, SYNC_LEN);
  Serial.println();

  setupRxWithAddress(ch, SYNC_WORD, SYNC_LEN);

  uint32_t start = millis();
  uint8_t buf[32];
  int count = 0;

  while (millis() - start < 30000) {
    if (radio.available()) {
      radio.read(buf, 32);
      Serial.print(F("RX["));
      if (count < 10) Serial.print(' ');
      Serial.print(count++);
      Serial.print(F("]: "));
      printHex(buf, 32);
      Serial.println();
    }
  }

  radio.stopListening();
  Serial.print(F("Received "));
  Serial.print(count);
  Serial.println(F(" valid frames."));

  if (count == 0) {
    Serial.println(F("Nothing received. Try:"));
    Serial.println(F("  - reversing byte order of SYNC_WORD"));
    Serial.println(F("  - different SYNC_LEN"));
    Serial.println(F("  - different RF_CHANNEL"));
    Serial.println(F("  - lowering data rate (edit setupRxWithAddress)"));
  }
}

static void transmitPayload(const uint8_t* p, size_t len, const char* label) {
  Serial.print(F("TX "));
  Serial.print(label);
  Serial.print(F(" ("));
  Serial.print(len);
  Serial.print(F(" bytes): "));
  printHex(p, len);

  setupTx();
  radio.setPayloadSize(len);

  // Burst 3 times, like the real remote does, for reliability
  for (int i = 0; i < 3; i++) {
    radio.write(p, len);
    delayMicroseconds(500);
  }

  Serial.println(F("[done]"));
}

static void showHelp() {
  Serial.println();
  Serial.println(F("NRF24 <-> LT8910 bridge. Commands:"));
  Serial.println(F("  s       RSSI scan all 84 channels (~20s)"));
  Serial.println(F("  m <n>   promiscuous RX on channel n for 30s"));
  Serial.println(F("  r <n>   sync-word RX on channel n for 30s"));
  Serial.println(F("  p       TX Power payload"));
  Serial.println(F("  u       TX Up payload"));
  Serial.println(F("  d       TX Down payload"));
  Serial.println(F("  c <n>   set TX channel to n"));
  Serial.println(F("  ?       this help"));
  Serial.println();
  Serial.print(F("Current: ch="));
  Serial.print(RF_CHANNEL);
  Serial.print(F(" sync="));
  printHex(SYNC_WORD, SYNC_LEN);
  Serial.print(F("(len="));
  Serial.print(SYNC_LEN);
  Serial.println(F(")"));
}

// --- Main -----------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(200);

  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F(" NRF24L01+ as LT8910 bridge — ThermaSleep project "));
  Serial.println(F("=================================================="));

  if (!radio.begin()) {
    Serial.println(F("ERROR: NRF24 not responding. Check wiring + 10uF cap on VCC."));
    while (1) delay(1000);
  }

  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(false);
  radio.disableCRC();

  showHelp();
}

void loop() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  char cmd = line[0];
  int arg = -1;
  if (line.length() > 1) {
    arg = line.substring(1).toInt();
  }

  switch (cmd) {
    case 's': case 'S':
      rssiScanAll();
      break;

    case 'm': case 'M':
      if (arg < 0 || arg > 83) {
        Serial.println(F("Usage: m <channel 0-83>"));
      } else {
        promiscuousOnChannel((uint8_t)arg);
      }
      break;

    case 'r': case 'R':
      if (arg < 0 || arg > 83) {
        Serial.println(F("Usage: r <channel 0-83>"));
      } else {
        listenChannel((uint8_t)arg);
      }
      break;

    case 'c': case 'C':
      if (arg < 0 || arg > 83) {
        Serial.println(F("Usage: c <channel 0-83>"));
      } else {
        RF_CHANNEL = (uint8_t)arg;
        Serial.print(F("TX channel set to "));
        Serial.println(RF_CHANNEL);
      }
      break;

    case 'p': case 'P':
      transmitPayload(POWER_PAYLOAD, sizeof(POWER_PAYLOAD), "Power");
      break;
    case 'u': case 'U':
      transmitPayload(UP_PAYLOAD, sizeof(UP_PAYLOAD), "Up");
      break;
    case 'd': case 'D':
      transmitPayload(DOWN_PAYLOAD, sizeof(DOWN_PAYLOAD), "Down");
      break;

    case '?': case 'h': case 'H':
      showHelp();
      break;

    default:
      Serial.print(F("Unknown: "));
      Serial.println(cmd);
      showHelp();
  }
}

/*
 * Discovery workflow (once NRF24 and ESP32 arrive):
 *
 * 1. Wire up NRF24 per the pin table above. Flash this sketch.
 *
 * 2. Run `s` — RSSI scan. Pod should light up one specific channel.
 *    Note the channel number and edit RF_CHANNEL at the top.
 *
 * 3. Run `m <ch>` — promiscuous capture on that channel. Press remote
 *    buttons repeatedly. Capture 20+ frames to captures/*.txt
 *
 * 4. Analyse captures: look for 2-5 bytes that are IDENTICAL across all
 *    frames (the sync word). The bytes that follow are the payload.
 *
 * 5. Edit SYNC_WORD[] and SYNC_LEN at the top with your discovered sync.
 *    Re-flash.
 *
 * 6. Run `r <ch>` — should now cleanly receive frames with the real sync
 *    word. Record payloads per button into decode.md.
 *
 * 7. Edit POWER/UP/DOWN _PAYLOAD[] with the captures. Re-flash.
 *
 * 8. Run `p` / `u` / `d` — pod should respond as if you pressed the
 *    real remote. That's Phase 2.5 complete — move to ESPHome gateway.
 *
 * If step 3 yields nothing across several channel guesses, the LT8910
 * is probably using whitening (datasheet reg 41 bit 13). NRF24 can't
 * de-whiten in hardware — you'd need to capture with SPI bus tap or
 * switch to an actual LT8910 module.
 */
