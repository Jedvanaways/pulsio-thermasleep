/*
 * ESP32 smoke test for the ThermaSleep project.
 *
 * First-boot verification that your Binghe ESP32 (or any ESP32-WROOM-32D)
 * is healthy before wiring up the radio hardware. Checks:
 *
 *   1. CPU / chip ID / MAC — proves the ESP32 is what we expected
 *   2. Flash size + free heap — sanity-check memory
 *   3. Onboard LED blink on GPIO 2 — proves GPIO works
 *   4. WiFi scan — proves the 2.4GHz radio works (no password / credentials
 *      needed, this just listens for nearby SSIDs)
 *   5. Continuous heartbeat loop — proves it's stable over time
 *
 * Flash this first. If it runs cleanly for a minute you know the board is
 * good and we can move to the NRF24 / LT8910 sketches with confidence.
 *
 * Flashing (Arduino IDE 2.x):
 *   1. File > Preferences > Additional Boards Manager URLs, add:
 *        https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *   2. Tools > Board > Boards Manager, search "esp32", install "esp32 by Espressif".
 *   3. Tools > Board > esp32 > "ESP32 Dev Module"
 *   4. Tools > Port > /dev/cu.wchusbserial* (macOS) or COMx (Windows)
 *   5. Upload.
 *   6. Tools > Serial Monitor at 115200 baud.
 *
 * If the port doesn't appear on macOS, install the CH340 driver from
 * https://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html (unplug/replug
 * after install). Modern macOS usually works without it.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_flash.h>

static const uint8_t LED_PIN = 2;   // onboard LED on most ESP32 dev boards

static void printChipInfo() {
  Serial.println();
  Serial.println(F("================================================="));
  Serial.println(F(" ESP32 smoke test — ThermaSleep project"));
  Serial.println(F("================================================="));

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  Serial.print(F("CPU model     : "));
  switch (chip_info.model) {
    case CHIP_ESP32:    Serial.println(F("ESP32")); break;
    case CHIP_ESP32S2:  Serial.println(F("ESP32-S2")); break;
    case CHIP_ESP32S3:  Serial.println(F("ESP32-S3")); break;
    case CHIP_ESP32C3:  Serial.println(F("ESP32-C3")); break;
    default:            Serial.printf("unknown (%d)\n", chip_info.model);
  }
  Serial.print(F("Cores         : "));  Serial.println(chip_info.cores);
  Serial.print(F("Revision      : "));  Serial.println(chip_info.revision);
  Serial.print(F("Features      : "));
  if (chip_info.features & CHIP_FEATURE_WIFI_BGN) Serial.print(F("WiFi "));
  if (chip_info.features & CHIP_FEATURE_BT)       Serial.print(F("BT "));
  if (chip_info.features & CHIP_FEATURE_BLE)      Serial.print(F("BLE "));
  if (chip_info.features & CHIP_FEATURE_EMB_FLASH)Serial.print(F("EmbFlash "));
  Serial.println();

  uint32_t flash_size = 0;
  esp_flash_get_size(NULL, &flash_size);
  Serial.print(F("Flash size    : "));
  Serial.print(flash_size / (1024 * 1024));
  Serial.println(F(" MB"));

  Serial.print(F("Free heap     : "));
  Serial.print(ESP.getFreeHeap());
  Serial.println(F(" bytes"));

  Serial.print(F("MAC           : "));
  Serial.println(WiFi.macAddress());

  Serial.print(F("SDK version   : "));
  Serial.println(ESP.getSdkVersion());

  Serial.println();
}

static void blinkTest() {
  Serial.println(F("Blinking onboard LED on GPIO 2 (3x)..."));
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(200);
    digitalWrite(LED_PIN, LOW);  delay(200);
  }
  Serial.println(F("  -> if you saw it blink, GPIO output works."));
  Serial.println(F("  -> if not, your board's onboard LED may be on a different pin"));
  Serial.println(F("     (GPIO 1, 5, or 22 on some variants). Non-fatal for this project."));
  Serial.println();
}

static void wifiScan() {
  Serial.println(F("Scanning for nearby WiFi networks (proves the 2.4GHz radio works)..."));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println(F("  -> 0 networks found. Unusual but not necessarily broken."));
    Serial.println(F("     The radio responded to the scan, so hardware is likely OK."));
  } else {
    Serial.printf("  -> found %d network(s):\n", n);
    for (int i = 0; i < n && i < 10; i++) {
      Serial.printf("     [%2d] %-32s  RSSI %4d dBm  ch %2d\n",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    WiFi.channel(i));
    }
  }
  WiFi.scanDelete();
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  while (!Serial && millis() < 3000) delay(10);

  printChipInfo();
  blinkTest();
  wifiScan();

  Serial.println(F("================================================="));
  Serial.println(F(" Smoke test complete. Board is alive."));
  Serial.println(F(" Heartbeat loop below — watch for stable output."));
  Serial.println(F("================================================="));
  Serial.println();
}

void loop() {
  static uint32_t ticks = 0;
  static uint32_t last = 0;
  uint32_t now = millis();

  if (now - last >= 2000) {
    last = now;
    Serial.printf("[%5lu] alive, uptime=%lus, free heap=%u bytes\n",
                  ticks++, now / 1000, (unsigned)ESP.getFreeHeap());

    // Toggle LED each heartbeat so you can see it's looping even if serial
    // is unplugged.
    digitalWrite(LED_PIN, ticks & 1);
  }
}
