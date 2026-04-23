# ThermaSleep RF protocol — decoded

Fill this in during Phase 2. The radio chip is **LT8910** (Linktekco), NOT nRF24 — see [CHIP_IDENTIFICATION.md](CHIP_IDENTIFICATION.md).

Everything here becomes the input to `tools/replay-test.ino` and `esphome/thermasleep_radio.h`.

## Radio parameters (LT8910)

| Parameter | Value | Notes |
|---|---|---|
| Carrier | 2.4 GHz ISM | |
| Channel | `__` | LT8910 register 7 (`R_CHANNEL`). 0–83, frequency = 2400 + n MHz. |
| Data rate | `__` | LT8910 register 44, value 1/2/3/4 → 1Mbps / 250kbps / 125kbps / 62.5kbps. Try 1 Mbps first. |
| Preamble length | `__` bytes | LT8910 reg 32, default `0x4856` (ish). Usually 4 bytes of `0x55`. |
| Sync word width | `__` bits | LT8910 reg 27-28-29-30 (32 to 64-bit syncword). |
| Sync word | `0x________________` | The killer field — without this the LT8910 won't receive anything. |
| CRC | `on / off` | LT8910 reg 41 bit 14. Default on. |
| Auto-ACK / FEC | `__` | Usually off for one-way remotes. |
| Packet length | `__` bytes | Header byte (length) + payload. Up to 256 bytes. |

## How we discovered the sync word

Cross out methods that didn't work, fill in the one that did:

- [ ] EEPROM dump revealed it at offset `0x__`
- [ ] SPI bus tap on U4 captured the register write at boot: `wr_reg(27, 0x__)`, etc.
- [ ] LT8910 default `0x7654` worked
- [ ] Brute-force scan landed on `0x____` after ___ minutes

## Captured payloads

Raw hex per button. The LT8910 receives `[length-byte] [payload-bytes]` after sync match.

### Power

```
<10 captures>
__ __ __ __ __
```

### Up

```
__ __ __ __ __
```

### Down

```
__ __ __ __ __
```

## Byte-by-byte analysis

| Byte index | Power value(s) | Up value(s) | Down value(s) | Interpretation |
|---|---|---|---|---|
| `[0]` | `0x__` | `0x__` | `0x__` | length byte? fixed header? |
| `[1]` | `0x__` | `0x__` | `0x__` | button opcode? |
| `[2]` | varies | varies | varies | counter? |
| `[3]` | ... | ... | ... | |
| `[4]` | ... | ... | ... | CRC (LT8910 hardware-handles if enabled) |

## Counter behavior

- Does any byte increment by 1 on each press of the *same* button? `yes/no`
- If yes: byte index `[_]`, starting value `0x__`, wrap behavior `0x__ → 0x__`
- Persistent across battery removal? `yes/no` (suggests EEPROM-backed counter)
- Shared across buttons or per-button? `shared / per-button`

## Final transmit-ready spec

```text
LT8910 register init:
  channel = N
  datarate = 1 (1 Mbps) / 2 (250 kbps) / etc.
  syncword = 0x________________  (32-bit lower / upper as required)
  preamble = 4 bytes 0x55
  CRC = on

POWER_PAYLOAD = { 0x__, 0x__, 0x__, 0x__, 0x__ }
UP_PAYLOAD    = { 0x__, 0x__, 0x__, 0x__, 0x__ }
DOWN_PAYLOAD  = { 0x__, 0x__, 0x__, 0x__, 0x__ }

# Byte [_] is the counter — ESP32 must increment + persist to NVS
# Byte [_..] is the CRC — LT8910 chip handles if CRC enabled in reg 41
```

## Open questions / notes

- (Anything weird you notice while capturing.)
