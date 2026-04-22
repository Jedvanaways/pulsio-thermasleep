# ThermaSleep RF protocol — decoded

Fill this in during Phase 2. Everything here becomes the input to `tools/replay-test.ino` and `esphome/thermasleep_radio.h`.

## Radio parameters

| Parameter | Value | Notes |
|---|---|---|
| Carrier | 2.4 GHz ISM | |
| Channel | `__` | Channel number `n` → frequency `2400 + n` MHz. From the scanner's `CH:` field. |
| Data rate | `__` | 250 kbps / 1 Mbps / 2 Mbps. Try 1 Mbps first. |
| Address width | `__` bytes | 3, 4, or 5. Usually 5. |
| Address | `0x__ __ __ __ __` | From scanner's `A:` field. **MSB first.** |
| CRC | `__` | `CRC_16` / `CRC_8` / `off`. Default for nRF24 is CRC-16-CCITT over address+payload, init 0xFFFF. |
| Auto-ACK | `__` | Usually `off` for one-way remotes. |
| Payload length | `__` bytes | Often 5, 8, 16, or 32. |

## Captured payloads

Raw hex per button, stripped of preamble + address (just the payload that the nRF24 FIFO hands us).

### Power

```
<paste 10 captures here, one per line>
__ __ __ __ __
__ __ __ __ __
...
```

### Up

```
__ __ __ __ __
...
```

### Down

```
__ __ __ __ __
...
```

## Byte-by-byte analysis

| Byte index | Power value(s) | Up value(s) | Down value(s) | Interpretation |
|---|---|---|---|---|
| `[0]` | `0x__` | `0x__` | `0x__` | fixed header? |
| `[1]` | `0x__` | `0x__` | `0x__` | button opcode? |
| `[2]` | varies | varies | varies | counter? CRC? |
| `[3]` | ... | ... | ... | |
| `[4]` | ... | ... | ... | |

## Counter behavior

- Does any byte increment by exactly 1 on each press of the *same* button? `yes/no`
- If yes, byte index: `[_]`, starting value seen: `0x__`, wrap-around: `0x__ → 0x__`
- Does the counter value carry across buttons, or is each button's counter independent? `<one/shared>`
- Does it persist across battery removal? `<check by pulling battery, pressing, re-capturing>`

## CRC

- Algorithm: `<CRC-16-CCITT / CRC-8 / unknown>`
- Init value: `0x____`
- Covers: `<address + payload / payload only>`
- How to validate: compute CRC over (address+payload-minus-last-2-bytes), compare to last 2 bytes of payload.

## Final payload spec

Once analysis is done, summarize in transmit-ready form:

```text
POWER_PAYLOAD = { 0x__, 0x__, 0x__, 0x__, 0x__ }   // length N
UP_PAYLOAD    = { 0x__, 0x__, 0x__, 0x__, 0x__ }
DOWN_PAYLOAD  = { 0x__, 0x__, 0x__, 0x__, 0x__ }

# Byte [_] is the counter — ESP32 should increment it each send and persist to NVS
# Byte [_..] is the CRC — ESP32 hardware (nRF24 chip itself) computes this; do not include in FIFO write
```

## Open questions / notes

- (Use this space for anything weird you notice during capture.)
