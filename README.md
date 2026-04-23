# Pulsio ThermaSleep → Home Assistant

Bring the Pulsio ThermaSleep water pod under Home Assistant control by reverse-engineering its 2.4GHz remote protocol and building an ESP32-based virtual remote.

The physical remote is **never modified** — it stays fully functional alongside the HA bridge.

> ## ⚠ Plan revision 2026-04-23 — read [CHIP_IDENTIFICATION.md](CHIP_IDENTIFICATION.md)
> High-resolution photos revealed the RF chip is a **Linktekco LT8910**, NOT an nRF24L01+ family chip as originally assumed. This invalidates the Crazyradio / nRF24L01+ sniffing path. The replacement plan uses an LT8910 module. The original NRF24-based file content below has been updated, but read CHIP_IDENTIFICATION.md first for the full pivot.

## What this is

- The ThermaSleep remote (board `FX-CD-Z4B-Y11`) transmits 3 commands (Power / Up / Down) over a proprietary 2.4GHz RF link using a **Linktekco LT8910** transceiver (chip marked `NST LT8910SSC`).
- The MCU is an unidentified Chinese 8-bit chip (`RD8F12AQ2205A`).
- Paired remote IDs are stored in the remote's FT24C02A I²C EEPROM.
- The plan: discover the LT8910 sync word + channel + payload format (via SPI bus tap, EEPROM dump, or sync-word brute force), then transmit the same packets from an ESP32 + LT8910 module connected to Home Assistant.

Four phases, each with a go/no-go gate.

## Hardware you'll need (revised for LT8910)

**Sniffer / discovery** (pick one or more):
- **Logic analyzer** (Saleae clone, ~£10) — for SPI bus tap on U4. The most reliable way to discover sync word + channel + payload format. Requires soldering thin wires to U4's SPI pins.
- **LT8910 module** (~£3-5 from AliExpress, 2-3 week ship) — for RSSI scanning to find the channel, and brute-force sync word search.
- **LT8920 module** (sister chip) — has a RAW RX mode that allows actual packet sniffing. Better than LT8910 if available.
- **HackRF / SDR** (~£200+) — universal solution if you want to capture & decode in software (gqrx + GNU Radio + custom GFSK decoder). Heavyweight.

**HA gateway (always required)**:
- ESP32 dev kit with pre-soldered headers (e.g. Binghe ESP-WROOM-32D)
- LT8910 module on SPI
- Half-size breadboard + Dupont jumpers
- USB-C cable

**No longer useful for this project**:
- The nRF24L01+ modules — wrong RF protocol. Save them for unrelated projects.
- The Crazyradio PA — don't buy. Wrong chip family.

Total revised: **~£15** for the gateway hardware (already mostly ordered). LT8910 module + logic analyzer add ~£15 more.

## Work order

1. **Prep (1 hour)** — buy/flash sniffing hardware. See [`tools/install-bastille-macos.sh`](tools/install-bastille-macos.sh).
2. **Phase 1 — Sniff (2–4 hours)** — capture 10+ packets per button. Save hex to `captures/power.txt` / `up.txt` / `down.txt`.
3. **Phase 2 — Decode (2–6 hours)** — identify address, channel, payload, CRC, counter. Fill in [`decode.md`](decode.md).
4. **Phase 2.5 — Replay test (1 hour)** — prove the decode by sending one captured packet from an ESP32+nRF24 and watching the pod react. Use [`tools/replay-test.ino`](tools/replay-test.ino).
5. **Phase 3 — Build HA gateway (2–4 hours)** — finalize [`esphome/thermasleep.yaml`](esphome/thermasleep.yaml) + [`esphome/thermasleep_radio.h`](esphome/thermasleep_radio.h), flash it, plug it in near the pod.
6. **Phase 4 — HA (30 min)** — accept the auto-discovered ESPHome device, build a Lovelace card, add automations.

## Phase 1 — Discover the LT8910 parameters

The LT8910 has no promiscuous mode — it only receives packets matching a known sync word. So Phase 1 is split into two halves:

### 1a. Find the channel (cheap, no soldering)

With an ESP32 + LT8910 module wired up, run `tools/replay-test.ino` and use the `s` (scan) command. It loops through all 84 channels reading RSSI; press buttons on the remote during the scan and look for a channel that lights up (RSSI ≥ ~30/31). That's the pod's channel.

### 1b. Find the sync word + payload (hard part)

Three options, in order of reliability:

**Option A — SPI bus tap on U4 (most reliable):**
1. Solder 4 thin (30 AWG) wires to U4's SPI pins: SCK, MOSI, MISO, SS. Refer to the LT8910 datasheet for SSOP-16 pinout.
2. Hook each to a logic analyzer (Saleae Logic 8 clone, £10).
3. Power the remote up — the MCU will configure the LT8910 in the first ~10ms after boot, writing the sync word into LT8910 register 27 (and friends).
4. Decode the SPI capture in PulseView or Saleae Logic software. Look for `wr_reg(27, ...)` writes — that's the sync word.
5. Press a button — capture the `wr_reg(50, ...)` calls (TX FIFO) to see the actual payload bytes.

**Option B — EEPROM dump (no soldering, may not contain sync word):**
Run `./tools/dump-eeprom.sh`. The 256-byte EEPROM dump might contain the sync word in plaintext if Pulsio stored it there for runtime config. Look for stable 4-byte values surrounded by `0xFF` regions.

**Option C — Brute force the sync word:**
Try LT8910 default sync word `0x7654` first. If that doesn't work, write a sketch that loops through all 65,536 possible 16-bit sync words at the discovered channel, attempting RX for ~100ms each. Total time: ~2 hours. Tedious but solder-free.

Once you have a channel + sync word that produces packets, save raw hex captures to `captures/power.txt` etc.

## Phase 2 — Decode

Open [`decode.md`](decode.md) and fill it in. Key steps:

1. Line up 10 captures for the same button — find the bytes that change.
2. Compare Power vs Up vs Down captures — usually a single byte differs.
3. If a byte increments per press → it's a counter. Note the starting value.
4. If CRC doesn't match CRC-16-CCITT over address+payload, try CRC-8 / custom.

## Phase 2.5 — Replay test (go/no-go gate)

Before building the full HA gateway, prove you understand the protocol by replaying one captured packet.

1. Wire ESP32 + nRF24L01+ as per [`docs/wiring.md`](#wiring) (same table used by the gateway).
2. Open [`tools/replay-test.ino`](tools/replay-test.ino) in Arduino IDE or PlatformIO.
3. Fill in `RF_CHANNEL`, `RF_ADDRESS`, and the three `*_PAYLOAD` arrays from `decode.md`.
4. Flash. Open Serial Monitor at 115200 baud. Power the pod on.
5. Type `p` + Enter → pod should react as if Power was pressed. Type `u` → Up. Type `d` → Down.

**Pod responds:** decode is correct, proceed to Phase 3.
**Pod ignores the ESP32 but obeys the real remote:** likely rolling code / encrypted counter. See [Fallback](#fallback).

## Phase 3 — Build HA gateway

The same ESP32 + nRF24L01+ becomes the permanent HA bridge.

1. Install ESPHome CLI on the Mac: `pip install esphome` (or use the HA add-on).
2. Edit [`esphome/thermasleep.yaml`](esphome/thermasleep.yaml):
   - `wifi` SSID/password (or use `!secret`)
   - `api` encryption key (`esphome secrets generate`)
3. Edit [`esphome/thermasleep_radio.h`](esphome/thermasleep_radio.h):
   - Paste the same `RF_CHANNEL`, `RF_ADDRESS`, `*_PAYLOAD` values from Phase 2.
4. Compile + flash:
   ```sh
   cd esphome
   esphome run thermasleep.yaml
   ```

## Phase 4 — Home Assistant

When the ESP32 boots onto WiFi, HA will surface a discovery notification for "thermasleep-bridge". Accept it — three button entities appear:

- `button.thermasleep_power`
- `button.thermasleep_up`
- `button.thermasleep_down`

### Lovelace card

```yaml
type: entities
title: Pulsio ThermaSleep
entities:
  - entity: button.thermasleep_power
    name: Power
  - entity: button.thermasleep_up
    name: Warmer
  - entity: button.thermasleep_down
    name: Cooler
```

### Useful scripts

`script.thermasleep_set_level` — call Up or Down N times with 500ms between presses to reach a target level, assuming the pod exposes discrete temperature steps.

```yaml
script:
  thermasleep_set_level:
    fields:
      delta:
        description: Positive to warm, negative to cool
    sequence:
      - repeat:
          count: "{{ (delta | int) | abs }}"
          sequence:
            - service: button.press
              target:
                entity_id: >
                  {{ 'button.thermasleep_up' if (delta | int) > 0
                     else 'button.thermasleep_down' }}
            - delay: "00:00:00.5"
```

## Wiring (LT8910 module to ESP32)

| LT8910 module pin | ESP32 pin |
|---|---|
| VCC  | 3V3 |
| GND  | GND |
| PKT (IRQ) | GPIO 2 (optional) |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| SCK  | GPIO 18 |
| RESET | GPIO 4 |
| SS (CSN) | GPIO 5 |

**Important**: LT8910 module pinout is NOT the same as nRF24L01+ — RESET and SS swap places. Check your module's silkscreen carefully. The LT8910 draws much less peak current than an nRF24 PA+LNA, so the ESP32's 3V3 rail can supply it directly without an external regulator/adapter board.

## Troubleshooting

### LT8910 module doesn't respond at boot

- Wiring: confirm SS and RESET are NOT swapped (common mistake — LT8910 differs from nRF24 here).
- Verify 3.3V power and GND. The module is 3.3V max; do NOT feed it 5V.
- Check the chip marking on your module is actually LT8910 (or LT8920) — eBay/AliExpress sellers occasionally swap chips.

### RSSI scan shows no peaks when remote is pressed

- Fresh batteries in the remote (3V coin cells).
- LED5 on the back of the remote should blink on each press — confirms RF is powered.
- Move the LT8910 module within 30 cm of the remote during scan.
- Re-run scan at different data rates (1 Mbps default, but try 250 kbps).

### Pod ignores replays from ESP32 but obeys real remote

Sync word is wrong, OR payload counter / CRC handling is wrong. Re-check the SPI bus tap capture for register writes you missed.

### ESP32 brownouts during TX

LT8910 draws ~30 mA peak in TX, well within the ESP32's 3V3 budget. If you're seeing brownouts it's something else — check USB cable and supply.

## Fallback

If Phase 2.5 fails — rolling code, encryption, or undecoded counter — the guaranteed fallback is **soldered button emulation**:

1. Open the remote's case.
2. Solder 3 fine wires to the K1/K2/K3 pads and one to GND.
3. Connect each to an ESP32 GPIO through an opto-isolator (e.g. PC817) — active-low signal momentarily shorts the pad to GND, simulating a finger press.
4. Re-use `esphome/thermasleep.yaml` but swap the `button` platform from `template` to `gpio` writes driving the optos.

This sacrifices the "no mod to the remote" goal but works for any remote protocol. ~2 hours of work.

## Project layout

```
.
├── README.md                       # this file
├── CHIP_IDENTIFICATION.md          # 2026-04-23 plan revision — read first
├── decode.md                       # Phase 2 protocol writeup (LT8910)
├── captures/                       # Phase 1 raw captures
│   ├── README.md
│   ├── power.txt
│   ├── up.txt
│   └── down.txt
├── tools/
│   ├── replay-test.ino             # ESP32 + LT8910 sanity-check + RSSI scanner
│   ├── dump-firmware.sh            # ST-Link MCU dump (probably won't work — MCU is unidentified Chinese)
│   ├── dump-eeprom.sh              # CH341A FT24C02A EEPROM dump
│   ├── check-logitech-receiver.ps1 # Windows: detect flashable Logitech dongles (legacy from NRF24 plan)
│   └── install-bastille-macos.sh   # legacy from NRF24 plan — kept for reference
└── esphome/
    ├── thermasleep.yaml            # Phase 3 HA gateway config
    ├── thermasleep_radio.h         # Phase 3 LT8910 helper
    └── secrets.yaml.example        # WiFi + API secrets template
```
