// ThermaSleep radio helper, included by thermasleep.yaml.
// Fill in the <<FILL>> values from decode.md after Phase 2.

#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

static constexpr uint8_t CE_PIN  = 4;
static constexpr uint8_t CSN_PIN = 5;

// -------- FILL FROM decode.md --------
static constexpr uint8_t RF_CHANNEL       = 0;                                   // <<FILL>>
static const     uint8_t RF_ADDRESS[5]    = {0x00, 0x00, 0x00, 0x00, 0x00};      // <<FILL>> MSB first
static constexpr rf24_datarate_e RF_RATE  = RF24_1MBPS;                          // <<FILL>>
static constexpr uint8_t RF_ADDRESS_WIDTH = 5;                                   // <<FILL>>

// Payloads — include the counter slot as 0x00; thermasleep_send() will overwrite
// the counter byte before transmit. If no counter is used, ignore the ctr arg.
static constexpr uint8_t COUNTER_BYTE_INDEX = 0xFF;                              // <<FILL>> 0xFF = no counter

static const uint8_t POWER_PAYLOAD[] = {0x00};                                   // <<FILL>>
static const uint8_t UP_PAYLOAD[]    = {0x00};                                   // <<FILL>>
static const uint8_t DOWN_PAYLOAD[]  = {0x00};                                   // <<FILL>>
// -------------------------------------

enum ThermaSleepButton : uint8_t {
  THERMASLEEP_POWER = 0,
  THERMASLEEP_UP    = 1,
  THERMASLEEP_DOWN  = 2,
};

static RF24 thermasleep_radio(CE_PIN, CSN_PIN);

inline void thermasleep_radio_setup() {
  if (!thermasleep_radio.begin()) {
    ESP_LOGE("thermasleep", "nRF24L01+ not responding — check wiring/power.");
    return;
  }
  thermasleep_radio.setChannel(RF_CHANNEL);
  thermasleep_radio.setDataRate(RF_RATE);
  thermasleep_radio.setAddressWidth(RF_ADDRESS_WIDTH);
  thermasleep_radio.setAutoAck(false);
  thermasleep_radio.setRetries(0, 0);
  thermasleep_radio.setPALevel(RF24_PA_HIGH);
  thermasleep_radio.openWritingPipe(RF_ADDRESS);
  thermasleep_radio.stopListening();
  ESP_LOGI("thermasleep", "Radio up: ch=%d rate=%d addr_w=%d",
           RF_CHANNEL, (int)RF_RATE, RF_ADDRESS_WIDTH);
}

inline void thermasleep_send(ThermaSleepButton btn, uint8_t ctr) {
  const uint8_t *src;
  size_t len;
  switch (btn) {
    case THERMASLEEP_POWER: src = POWER_PAYLOAD; len = sizeof(POWER_PAYLOAD); break;
    case THERMASLEEP_UP:    src = UP_PAYLOAD;    len = sizeof(UP_PAYLOAD);    break;
    case THERMASLEEP_DOWN:  src = DOWN_PAYLOAD;  len = sizeof(DOWN_PAYLOAD);  break;
    default: return;
  }

  uint8_t buf[32];
  memcpy(buf, src, len);
  if (COUNTER_BYTE_INDEX < len) buf[COUNTER_BYTE_INDEX] = ctr;

  // Transmit 3x back-to-back to match the real remote's typical burst pattern and
  // improve chances of being heard through WiFi interference.
  for (int i = 0; i < 3; i++) {
    thermasleep_radio.write(buf, len);
    delayMicroseconds(500);
  }

  ESP_LOGI("thermasleep", "TX btn=%d ctr=0x%02X len=%u", (int)btn, ctr, (unsigned)len);
}
