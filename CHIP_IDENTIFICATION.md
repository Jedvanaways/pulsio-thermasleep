# Chip identification (2026-04-23)

High-resolution photos of the remote PCB revealed the actual chip identities. This invalidates several assumptions in the original plan.

## U4 — RF transceiver

- **Marking**: `NST` / `LT8910SSC` / `2510`
- **Identity**: Linktekco LT8910 (sometimes branded "NST"). 2.4GHz GFSK transceiver in SSOP-16. Date code 2025-week-10.
- **Crystal Y1**: 12.000 MHz (matches LT8910's typical reference)
- **NOT** an nRF24L01+ family chip. **NOT** on-air compatible with nRF24.
- Same chip used in cheap drones (Cheerson CX-10, Eachine E010, Holy Stone H120D, etc.).

### What this changes

| Originally assumed | Actually |
|---|---|
| nRF24L01+ family (Crazyradio works) | LT8910 — different protocol entirely |
| Bastille / Mousejack tooling applies | Doesn't apply |
| Promiscuous sniff mode | None — sync word required to receive |
| Default 1 MHz channel hopping | 84 channels at 2400–2483 MHz, fixed channel typical |

### Useful resources for LT8910

- Datasheet: https://datasheet4u.com/datasheets/LDT/LT8910/904972 (also bundled in MINI-Qiang/LT8910 repo)
- Arduino driver: https://github.com/MINI-Qiang/LT8910
- Sister chip LT8920 has a RAW RX mode useful for sniffing — possible alternative
- DIY-Multiprotocol-TX-Module project (drone hobby) has years of LT8910 protocol implementations: https://github.com/pascallanger/DIY-Multiprotocol-TX-Module

## MCU2

- **Marking**: `RD8F12AQ2205A` / `★ 2507 ▴` (date code week 7 of 2025)
- **Package**: 44-pin LQFP
- **Identity**: Unidentified Chinese 8-bit MCU. "RD" prefix doesn't match any well-known vendor (RDA/Realtek/Renesas don't fit). Likely a domestic Chinese vendor with limited English-language docs.
- **Programming interface (CN2)**: pads labelled `+3V / GND / SCK / SDA / RESET`. Given this is almost certainly NOT an ARM Cortex-M chip, "SCK / SDA" likely means I²C-based ISP, not SWD.
- **Implication**: ST-Link almost certainly cannot talk to it. Phase 0 firmware-dump path is **probably dead**.

## IC2

- **Marking**: `FT24C02A` / `5B9CKE`
- **Identity**: 2 Kbit I²C EEPROM (256 bytes). Stores the paired remote's address / pairing config.
- Unchanged from previous analysis. CH341A SOIC-clip dump is still viable and would tell us the pairing data.

## Updated approach hierarchy

Given the LT8910 + unknown-MCU situation:

1. **EEPROM dump** (still works, no soldering with a SOIC clip)
   → Reveals pairing data, possibly the sync word/address used by the LT8910 link.
2. **SPI bus tap on U4** (requires fine soldering — violates solder-free goal)
   → Only reliable way to discover sync word + channel + data rate without firmware access.
   → Need a £10 Saleae logic analyzer clone + steady hand.
3. **Sync word brute-force** with an LT8910 module (slow but solder-free)
   → Try all 65,536 possible 16-bit sync words at each candidate channel. Tedious.
4. **Try LT8910 default sync word** (`0x7654` per datasheet) — might just work if Pulsio didn't customise it.
5. **SDR + GNU Radio** (HackRF £200+) — overkill for one device but the universal solution.

## Hardware that's now wrong

- The 4× nRF24L01+ modules: **wrong chip**, won't communicate with the pod. Keep them for unrelated projects.
- The Crazyradio PA: don't buy. Wrong chip family.

## Hardware that's still right

- ESP32 dev board: still the gateway brain. Will drive an LT8910 module via SPI, same way it would have driven nRF24.
- ST-Link V2: useful as a logic-analyzer-of-last-resort, but better to get a Saleae clone.

## Hardware still needed

- **An LT8910 module** — best source: AliExpress "LT8910 module" (£3-5, 2-3 week ship). Amazon US has bare LT8910SSC chips (£10 for 10) but they're SSOP-16 — needs fine soldering to a breakout.
- **OR** an LT8920 module (sister chip with RAW RX mode — better for sniffing)
- **A logic analyzer** — £10 Saleae clone from Amazon / AliExpress, or use the Pulseview-compatible version
- Optional: a £10 Cheerson CX-10 quadcopter — has an LT8910 in it that you can salvage with a hot air gun
