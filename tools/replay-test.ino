/*
 * ThermaSleep Phase 2.5 replay sanity-check sketch — LT8910 version.
 *
 * Hardware:
 *   - ESP32 dev kit (Binghe ESP-WROOM-32D or similar)
 *   - LT8910 module on SPI bus
 *
 * Library: install LT8910 from MINI-Qiang
 *   - Library Manager: search "LT8910"
 *   - Or: Sketch > Include Library > Add ZIP from
 *     https://github.com/MINI-Qiang/LT8910/archive/refs/heads/master.zip
 *
 * After Phase 2 decode, fill in the constants marked <<FILL>> and flash.
 * Open Serial Monitor @ 115200. Commands:
 *   p <Enter>  -> send Power
 *   u <Enter>  -> send Up
 *   d <Enter>  -> send Down
 *   s <Enter>  -> RSSI scan all 84 channels (find which channel the pod uses)
 *
 * Pod should react within ~1 second. If it doesn't, sync word or payload
 * is wrong.
 *
 * Wiring (ESP32 <-> LT8910 module — note pin labels differ from nRF24!):
 *
 *   LT8910 module pin   ESP32 GPIO
 *   ------------------- ----------
 *   VCC                 3V3
 *   GND                 GND
 *   PKT (a.k.a. IRQ)    GPIO 2  (optional)
 *   MISO                GPIO 19
 *   MOSI                GPIO 23
 *   SCK                 GPIO 18
 *   RESET               GPIO 4   (NOT SS like nRF24 — different layout!)
 *   SS   (a.k.a. CSN)   GPIO 5
 */

#include <SPI.h>
#include <LT8910.h>

// -------- FILL FROM decode.md --------
static const uint8_t  RF_CHANNEL    = 0;       // <<FILL>> 0-83 (2400 + n MHz)
static const uint8_t  RF_DATARATE   = LT8910::DATA_RATE_1MBPS;  // <<FILL>>
static const uint64_t RF_SYNCWORD   = 0x0000000000007654ULL;    // <<FILL>> LT8910 default for testing
static const uint8_t  RF_SYNCWIDTH  = 32;      // <<FILL>> 16/32/64 bits
static const bool     RF_CRC_ON     = true;    // <<FILL>>

static const uint8_t  POWER_PAYLOAD[]   = {0x00};   // <<FILL>>
static const uint8_t  UP_PAYLOAD[]      = {0x00};   // <<FILL>>
static const uint8_t  DOWN_PAYLOAD[]    = {0x00};   // <<FILL>>
// -------------------------------------

static const uint8_t SS_PIN     = 5;
static const uint8_t RESET_PIN  = 4;
static const uint8_t PKT_PIN    = 2;   // optional IRQ

LT8910 radio(SS_PIN, RESET_PIN, PKT_PIN);

static void sendPayload(const uint8_t *p, size_t len, const char *label) {
  Serial.print("TX ");
  Serial.print(label);
  Serial.print(" (");
  Serial.print(len);
  Serial.print(" bytes): ");
  for (size_t i = 0; i < len; i++) {
    if (p[i] < 0x10) Serial.print('0');
    Serial.print(p[i], HEX);
    Serial.print(' ');
  }
  bool ok = radio.sendPacket((uint8_t*)p, len);
  Serial.println(ok ? " [TX_DONE]" : " [TX_FAIL]");
}

static void scanChannels() {
  Serial.println("Scanning all 84 channels for RSSI activity...");
  Serial.println("Hold the remote and press buttons during scan to find its channel.");
  for (int ch = 0; ch < 84; ch++) {
    radio.setChannel(ch);
    delay(2);
    int rssi = radio.scanRSSI(ch, 1);
    if (rssi > 30) {  // threshold; LT8910 RSSI is 0-31 (see datasheet)
      Serial.print("  CH ");
      Serial.print(ch);
      Serial.print(" (");
      Serial.print(2400 + ch);
      Serial.print(" MHz): RSSI ");
      Serial.println(rssi);
    }
  }
  Serial.println("Scan done.");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(200);

  Serial.println("ThermaSleep LT8910 replay test");

  if (!radio.begin(RF_CHANNEL, RF_DATARATE)) {
    Serial.println("ERROR: LT8910 not responding. Check wiring + 3V3 power.");
    Serial.println("       Note: SS and RESET swap places vs nRF24 modules!");
    while (1) delay(1000);
  }
  radio.setSyncWord(RF_SYNCWORD, RF_SYNCWIDTH);
  if (RF_CRC_ON) radio.enableCRC(); else radio.disableCRC();

  Serial.print("Radio up: ch=");
  Serial.print(RF_CHANNEL);
  Serial.print(" rate=");
  Serial.print(RF_DATARATE);
  Serial.print(" sync=0x");
  Serial.print((uint32_t)(RF_SYNCWORD >> 32), HEX);
  Serial.println((uint32_t)(RF_SYNCWORD & 0xFFFFFFFF), HEX);

  Serial.println("Commands: p=Power, u=Up, d=Down, s=Scan");
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 'p': case 'P':
      sendPayload(POWER_PAYLOAD, sizeof(POWER_PAYLOAD), "Power");
      break;
    case 'u': case 'U':
      sendPayload(UP_PAYLOAD, sizeof(UP_PAYLOAD), "Up");
      break;
    case 'd': case 'D':
      sendPayload(DOWN_PAYLOAD, sizeof(DOWN_PAYLOAD), "Down");
      break;
    case 's': case 'S':
      scanChannels();
      break;
    case '\r': case '\n': case ' ':
      break;
    default:
      Serial.print("Unknown: ");
      Serial.println(c);
  }
}

/*
 * NOTE: the LT8910 Arduino library API may not exactly match the calls above
 * (sendPacket / scanRSSI / setSyncWord). The MINI-Qiang/LT8910 library has
 * the canonical names. Adjust if your version of the library uses different
 * method names. The library is small (~200 lines) and easy to read.
 */
