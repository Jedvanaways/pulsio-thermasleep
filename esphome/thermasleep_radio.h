// ThermaSleep radio helper — LT8910 version.
// The remote uses an LT8910 transceiver, NOT nRF24. See CHIP_IDENTIFICATION.md.
//
// This header is a placeholder skeleton. ESPHome doesn't have a built-in
// LT8910 component, and the MINI-Qiang/LT8910 Arduino library may need
// minor wrapping to compile under ESPHome's Arduino-platform build.
//
// Fill in the <<FILL>> values from decode.md after Phase 2.

#pragma once
#include <Arduino.h>
#include <SPI.h>
// #include <LT8910.h>   // uncomment after adding library to platformio.ini

static constexpr uint8_t SS_PIN    = 5;
static constexpr uint8_t RESET_PIN = 4;
static constexpr uint8_t PKT_PIN   = 2;

// -------- FILL FROM decode.md --------
static constexpr uint8_t  RF_CHANNEL   = 0;                              // <<FILL>>
static constexpr uint8_t  RF_DATARATE  = 1;                              // <<FILL>> 1=1Mbps, 2=250kbps...
static constexpr uint64_t RF_SYNCWORD  = 0x0000000000007654ULL;          // <<FILL>>
static constexpr uint8_t  RF_SYNCWIDTH = 32;                             // <<FILL>>

static constexpr uint8_t COUNTER_BYTE_INDEX = 0xFF;                      // <<FILL>> 0xFF = no counter

static const uint8_t POWER_PAYLOAD[] = {0x00};                           // <<FILL>>
static const uint8_t UP_PAYLOAD[]    = {0x00};                           // <<FILL>>
static const uint8_t DOWN_PAYLOAD[]  = {0x00};                           // <<FILL>>
// -------------------------------------

enum ThermaSleepButton : uint8_t {
  THERMASLEEP_POWER = 0,
  THERMASLEEP_UP    = 1,
  THERMASLEEP_DOWN  = 2,
};

inline void thermasleep_radio_setup() {
  // TODO: initialize LT8910 here once the Arduino library is wired in.
  // The MINI-Qiang/LT8910 lib's API:
  //   LT8910 radio(SS_PIN, RESET_PIN, PKT_PIN);
  //   radio.begin(RF_CHANNEL, RF_DATARATE);
  //   radio.setSyncWord(RF_SYNCWORD, RF_SYNCWIDTH);
  //   radio.enableCRC();
  // Wrap as needed for the ESPHome lambda context.
  ESP_LOGI("thermasleep", "TODO: initialize LT8910 (ch=%d rate=%d)",
           (int)RF_CHANNEL, (int)RF_DATARATE);
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

  uint8_t buf[64];
  memcpy(buf, src, len);
  if (COUNTER_BYTE_INDEX < len) buf[COUNTER_BYTE_INDEX] = ctr;

  // TODO: actually transmit via LT8910:
  //   for (int i = 0; i < 3; i++) {  // burst 3x like the real remote
  //     radio.sendPacket(buf, len);
  //     delayMicroseconds(500);
  //   }

  ESP_LOGI("thermasleep", "TX btn=%d ctr=0x%02X len=%u (LT8910 not yet wired)",
           (int)btn, ctr, (unsigned)len);
}
