/*
 * ThermaSleep Phase 2.5 replay sanity-check sketch.
 *
 * Hardware: ESP32 dev kit + nRF24L01+ PA+LNA module on socket adapter.
 * Library: maniacbug/RF24 (install via Library Manager — "RF24 by TMRh20")
 *
 * After Phase 2 decode, fill in the constants marked <<FILL>> and flash.
 * Open Serial Monitor @ 115200. Type:
 *   p <Enter>  → send Power
 *   u <Enter>  → send Up
 *   d <Enter>  → send Down
 *
 * Pod should react within ~1 second. If it doesn't, decode is wrong.
 *
 * Wiring (see README.md → Wiring):
 *   CE  → GPIO 4
 *   CSN → GPIO 5
 *   SCK → GPIO 18
 *   MOSI→ GPIO 23
 *   MISO→ GPIO 19
 *   VCC → 3V3 via PA+LNA adapter (adapter fed from 5V)
 *   GND → GND
 */

#include <SPI.h>
#include <RF24.h>

// -------- FILL FROM decode.md --------
static const uint8_t  RF_CHANNEL        = 0;                                     // <<FILL>> e.g. 47
static const uint8_t  RF_ADDRESS[5]     = {0x00, 0x00, 0x00, 0x00, 0x00};        // <<FILL>> MSB first
static const rf24_datarate_e RF_RATE    = RF24_1MBPS;                            // <<FILL>> RF24_250KBPS / RF24_1MBPS / RF24_2MBPS
static const uint8_t  RF_ADDRESS_WIDTH  = 5;                                     // <<FILL>> 3 / 4 / 5

static const uint8_t  POWER_PAYLOAD[]   = {0x00};                                // <<FILL>>
static const uint8_t  UP_PAYLOAD[]      = {0x00};                                // <<FILL>>
static const uint8_t  DOWN_PAYLOAD[]    = {0x00};                                // <<FILL>>
// -------------------------------------

static const uint8_t CE_PIN  = 4;
static const uint8_t CSN_PIN = 5;
RF24 radio(CE_PIN, CSN_PIN);

static void sendPayload(const uint8_t *p, size_t len, const char *label) {
  Serial.print("TX ");
  Serial.print(label);
  Serial.print(": ");
  for (size_t i = 0; i < len; i++) {
    if (p[i] < 0x10) Serial.print('0');
    Serial.print(p[i], HEX);
    Serial.print(' ');
  }
  bool ok = radio.write(p, len);
  Serial.println(ok ? " [ACK]" : " [noACK]");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(200);

  Serial.println("ThermaSleep replay test");
  if (!radio.begin()) {
    Serial.println("ERROR: nRF24 not responding. Check wiring + power (PA+LNA needs 5V adapter).");
    while (1) delay(1000);
  }
  radio.setChannel(RF_CHANNEL);
  radio.setDataRate(RF_RATE);
  radio.setAddressWidth(RF_ADDRESS_WIDTH);
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_HIGH);
  radio.openWritingPipe(RF_ADDRESS);
  radio.stopListening();
  radio.printPrettyDetails();

  Serial.println("Commands: p = Power, u = Up, d = Down");
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
    case '\r': case '\n': case ' ':
      break;
    default:
      Serial.print("Unknown command: ");
      Serial.println(c);
  }
}
