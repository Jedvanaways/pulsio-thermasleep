#!/usr/bin/env bash
# Phase 0 bonus — dump the FT24C02A EEPROM (IC2 on the remote PCB) to read
# the paired remote's on-air address directly.
#
# The FT24C02A is a 2Kbit (256-byte) I²C EEPROM. It's an 8-pin SOIC chip
# on the back of the board (near the bottom-right of the front side in
# your photos, labelled IC2 "FT24C02A 5B9CKE").
#
# Hardware needed:
#   - CH341A USB programmer (~£5, same programmer used for BIOS/SPI flash work)
#   - SOIC-8 test clip (~£5)
#   OR
#   - A few jumper wires soldered to the 4 useful pins (VCC/GND/SDA/SCL) —
#     but this violates the solder-free goal.
#
# The CH341A's SOIC clip lets you read IC2 in-circuit (no desolder required)
# as long as the rest of the remote is unpowered during the read.
#
# On macOS:
#   brew install ch341eeprom   # may need tap; alternative is flashrom
# Or use flashrom:
#   flashrom -p ch341a_spi -c "24C02" -r eeprom.bin
#
# Wiring (SOIC clip to IC2 — pin 1 is the dot-marked corner, usually bottom-left):
#   Pin 1 A0   → GND
#   Pin 2 A1   → GND
#   Pin 3 A2   → GND  (these three set the I²C address — usually tied to GND)
#   Pin 4 GND  → GND
#   Pin 5 SDA  → SDA
#   Pin 6 SCL  → SCL
#   Pin 7 WP   → GND (or leave, WP not enforced for reads)
#   Pin 8 VCC  → 3.3V (NOT 5V — FT24C02A is happy with 2.5-5.5V but remote runs 3V)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/firmware"
mkdir -p "$OUT"

echo "======================================================================"
echo "Phase 0 bonus: FT24C02A EEPROM dump"
echo "======================================================================"
echo
echo "Before running:"
echo "  1. Remove batteries from the remote (in-circuit read = unpowered target)."
echo "  2. Clip SOIC-8 onto IC2 with the red wire (pin 1) on the dot-marked corner."
echo "  3. Plug CH341A into the Mac."
echo
read -rp "Press Enter when clipped and plugged in..."

if command -v flashrom >/dev/null; then
  echo "Reading via flashrom..."
  flashrom -p ch341a_spi -c "AT24C02" -r "$OUT/eeprom.bin" || \
    flashrom -p ch341a_spi -c "24C02" -r "$OUT/eeprom.bin"
elif command -v ch341eeprom >/dev/null; then
  echo "Reading via ch341eeprom..."
  ch341eeprom -s 24c02 -r "$OUT/eeprom.bin"
else
  echo "Neither flashrom nor ch341eeprom is installed."
  echo "  brew install flashrom"
  exit 1
fi

echo
echo "EEPROM dumped → firmware/eeprom.bin"
echo
echo "Hex view:"
xxd "$OUT/eeprom.bin"
echo
echo "Interpretation tips:"
echo "  - FF FF FF FF ...    = blank/unused regions"
echo "  - 5-byte address     = likely the on-air RF address (nRF24 addresses are 3-5 bytes)"
echo "  - Look for non-FF islands of 5–8 consecutive bytes at stable offsets."
echo "  - Cross-reference with captured packets in Phase 1 — they should match."
