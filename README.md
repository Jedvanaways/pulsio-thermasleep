# Pulsio ThermaSleep → Home Assistant

Bring the Pulsio ThermaSleep water pod under Home Assistant control by reverse-engineering its 2.4GHz remote protocol and building an ESP32-based virtual remote.

The physical remote is **never modified** — it stays fully functional alongside the HA bridge.

## What this is

- The ThermaSleep remote (board `FX-CD-Z4B-Y11`) transmits 3 commands (Power / Up / Down) over a proprietary 2.4GHz RF link — most likely Nordic nRF24L01+ or a clone (Beken BK24xx / SI24R1 / XN297).
- Paired remote IDs are stored in the remote's FT24C02A I²C EEPROM — so each remote has a unique address.
- The plan: sniff the remote's packets over the air, decode the address + channel + payload, then transmit the same packets from an ESP32 + nRF24L01+ module connected to Home Assistant.

Four phases, each with a go/no-go gate.

## Hardware you'll need

**Sniffer** (pick one):
- [Crazyradio PA](https://www.bitcraze.io/products/crazyradio-pa/) — ~£30. The path of least resistance.
- ESP32 + nRF24L01+ PA+LNA + socket adapter — ~£13 total. Cheaper, but sniffing is slower and noisier.

**HA gateway (always required)**:
- ESP32 dev kit with pre-soldered headers (e.g. ESP32-DevKitC-V4)
- nRF24L01+ PA+LNA module **plus** a "socket adapter" / "SMD adapter" board (adds regulator + decoupling caps — stock modules brown out the ESP32's 3.3V rail)
- Half-size breadboard
- Female-to-female Dupont jumper wires
- USB cable + 5V phone charger

Total: **~£50** with Crazyradio, **~£18** ESP32-only.

## Work order

1. **Prep (1 hour)** — buy/flash sniffing hardware. See [`tools/install-bastille-macos.sh`](tools/install-bastille-macos.sh).
2. **Phase 1 — Sniff (2–4 hours)** — capture 10+ packets per button. Save hex to `captures/power.txt` / `up.txt` / `down.txt`.
3. **Phase 2 — Decode (2–6 hours)** — identify address, channel, payload, CRC, counter. Fill in [`decode.md`](decode.md).
4. **Phase 2.5 — Replay test (1 hour)** — prove the decode by sending one captured packet from an ESP32+nRF24 and watching the pod react. Use [`tools/replay-test.ino`](tools/replay-test.ino).
5. **Phase 3 — Build HA gateway (2–4 hours)** — finalize [`esphome/thermasleep.yaml`](esphome/thermasleep.yaml) + [`esphome/thermasleep_radio.h`](esphome/thermasleep_radio.h), flash it, plug it in near the pod.
6. **Phase 4 — HA (30 min)** — accept the auto-discovered ESPHome device, build a Lovelace card, add automations.

## Phase 1 — Sniff

With Crazyradio PA:

```sh
# Flash firmware (one-time)
./tools/install-bastille-macos.sh

# Scan all 2.4GHz channels for any nRF24 activity
cd nrf-research-firmware/tools
./nrf24-scanner.py -c 2-83
```

Put fresh batteries in the remote. Hold it within 30cm of the Crazyradio. Press **Power** 10 times — the scanner should print hex lines like:

```
[2025-04-22 20:34:12]  CH:  47   A:FA:14:7B:22:33   CRC:OK   P:aa 55 01 00 1c
```

- `CH` = channel — write it down, should be the same for all 3 buttons
- `A:` = on-air address — write it down
- `P:` = payload hex — copy each line into `captures/power.txt`

Repeat for **Up** → `captures/up.txt`, **Down** → `captures/down.txt`.

**If the scanner catches nothing in 5 minutes**, try narrower ranges (`-c 2-20`, `-c 70-83`), or switch to `./nrf24-sniffer.py -a FA:14:7B:22:33 -c <n>` once you've seen one packet.

**If still nothing after 30 minutes**, see the [Troubleshooting](#troubleshooting) section.

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

## Wiring

| nRF24L01+ pin | ESP32 pin |
|---|---|
| VCC | 3V3 (via PA+LNA socket adapter's own regulator) |
| GND | GND |
| CE  | GPIO 4 |
| CSN | GPIO 5 |
| SCK | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| IRQ | GPIO 2 (optional) |

**Important**: Do not feed 3.3V directly to a bare nRF24L01+ module. ESP32's 3.3V LDO cannot supply the ~100mA peaks the module draws in TX. Use a PA+LNA module on its socket adapter (has its own AMS1117 regulator + 10µF cap), fed from 5V. Without this you'll get mysterious resets and failed transmissions.

## Troubleshooting

### Scanner sees nothing

- Confirm the remote is actually transmitting: fresh batteries, LED blinks on button press (if LED5 on the back lights up, RF is powered).
- Try a borrowed RTL-SDR with a 2.4GHz up/downconverter or a HackRF in survey mode — should see visible bursts on button press.
- Beken BK24xx chips use 2-byte preamble (not nRF's 1-byte). Bastille scanner handles both, but the ESP32-based sniffer alternative may not — see the Crazyradio path instead.
- Check data rate — Chinese remotes often use 250 kbps, not 1 Mbps or 2 Mbps. Re-run scanner at each rate.

### Replay doesn't trigger the pod, real remote does

See [Fallback](#fallback) — protocol has anti-replay (rolling code or encrypted counter).

### ESP32 resets mid-transmit

Power issue with the nRF24. Get the PA+LNA socket adapter; do not run a bare nRF24L01+ off the ESP32's 3V3 rail.

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
├── decode.md                       # Phase 2 protocol writeup
├── captures/                       # Phase 1 raw captures
│   ├── README.md
│   ├── power.txt
│   ├── up.txt
│   └── down.txt
├── tools/
│   ├── install-bastille-macos.sh   # one-shot Crazyradio PA setup
│   └── replay-test.ino             # Phase 2.5 ESP32 sanity-check sketch
└── esphome/
    ├── thermasleep.yaml            # Phase 3 HA gateway config
    ├── thermasleep_radio.h         # Phase 3 radio + payload constants
    └── secrets.yaml.example        # WiFi + API secrets template
```
