#!/usr/bin/env bash
# Phase 0 — attempt to dump MCU2 firmware via SWD or SWIM.
#
# The CN2 header on the remote (+3V / GND / SCK / SDA / RESET) is almost
# certainly a factory programming port. Depending on what MCU2 actually is:
#   - ARM Cortex-M (STM32/GD32/HC32): SCK=SWCLK, SDA=SWDIO → use st-link SWD mode
#   - STM8S/STM8L:                    SDA=SWIM (single wire) → use stm8flash SWIM mode
#
# This script tries SWD first, then SWIM. Whichever responds tells you the chip.
#
# Prereqs on macOS:
#   brew install stlink stm8flash
#
# Wiring (ST-Link V2 clone ↔ CN2):
#   SWD mode:  SWCLK↔SCK,  SWDIO↔SDA,  3.3V↔+3V,  GND↔GND,  NRST↔RESET
#   SWIM mode: SWIM↔SDA,   NRST↔RESET, 3.3V↔+3V,  GND↔GND
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/firmware"
mkdir -p "$OUT"

echo "======================================================================"
echo "Phase 0: MCU firmware dump via ST-Link"
echo "======================================================================"
echo
echo "Before plugging in: put the ST-Link programmer in SWD mode (not SWIM)."
echo "Wire SWD mode first — most likely MCU is ARM Cortex-M."
echo
read -rp "Press Enter when wired and ST-Link is plugged into the Mac..."

echo
echo "[1/3] Probing for ARM Cortex-M via SWD..."
if command -v st-info >/dev/null; then
  if st-info --probe 2>&1 | tee "$OUT/probe-swd.log" | grep -q "chipid"; then
    CHIPID=$(grep -o 'chipid: *0x[0-9a-fA-F]*' "$OUT/probe-swd.log" | head -1)
    FLASH=$(grep -o 'flash: *[0-9]*' "$OUT/probe-swd.log" | head -1 | awk '{print $2}')
    FLASH=${FLASH:-65536}
    echo "  Found ARM chip: $CHIPID, flash bytes: $FLASH"
    echo "[2/3] Dumping flash..."
    if st-flash read "$OUT/firmware-swd.bin" 0x08000000 "$FLASH" 2>&1 | tee -a "$OUT/probe-swd.log"; then
      BYTES=$(wc -c <"$OUT/firmware-swd.bin" | tr -d ' ')
      NONFF=$(tr -d '\377' <"$OUT/firmware-swd.bin" | wc -c | tr -d ' ')
      echo "  Dumped $BYTES bytes to firmware/firmware-swd.bin"
      echo "  Non-0xFF bytes: $NONFF  (if close to 0, chip is readout-protected)"
      if [ "$NONFF" -lt 1000 ]; then
        echo "  WARNING: dump looks like all 0xFF — chip is likely RDP-protected."
        echo "  Falling back to over-the-air RF sniffing is the next path."
      else
        echo
        echo "SUCCESS. Load firmware/firmware-swd.bin into Ghidra (ARM Cortex-M)."
        echo "Search for nRF24 SPI opcodes: 0xA0 (W_TX_PAYLOAD), 0x2A (RX_ADDR_P0),"
        echo "0x25 (RF_CH), 0x20 (CONFIG). The bytes written right before 0xA0 are"
        echo "the packet payload — the protocol falls out of that code path."
      fi
      exit 0
    fi
  fi
  echo "  SWD probe did not detect a chip."
else
  echo "  st-info not found. Install: brew install stlink"
fi

echo
echo "----------------------------------------------------------------------"
echo "[3/3] SWD failed. Rewire ST-Link into SWIM mode and try STM8..."
echo "      SWIM wiring: SWIM↔SDA, NRST↔RESET, 3.3V↔+3V, GND↔GND"
echo "      Leave SCK↔?? unconnected (SWIM is a single-wire bus)."
echo "----------------------------------------------------------------------"
read -rp "Press Enter when rewired..."

if ! command -v stm8flash >/dev/null; then
  echo "stm8flash not installed. Install: brew install stm8flash"
  exit 1
fi

echo "Probing STM8 part variants in sequence (stm8s207, stm8s105, stm8l152, stm8s003)..."
for PART in stm8s207 stm8s105 stm8l152 stm8s003 stm8s003f3 stm8s105c6; do
  echo "  Trying -p $PART"
  if stm8flash -c stlinkv2 -p "$PART" -s flash -r "$OUT/firmware-stm8-$PART.bin" 2>&1 | tee "$OUT/probe-stm8-$PART.log" | grep -q "Bytes received"; then
    BYTES=$(wc -c <"$OUT/firmware-stm8-$PART.bin" | tr -d ' ')
    NONFF=$(tr -d '\377' <"$OUT/firmware-stm8-$PART.bin" | wc -c | tr -d ' ')
    echo "  Dumped $BYTES bytes as $PART → firmware/firmware-stm8-$PART.bin"
    echo "  Non-0xFF bytes: $NONFF"
    if [ "$NONFF" -gt 1000 ]; then
      echo
      echo "SUCCESS. Load firmware-stm8-$PART.bin into Ghidra (STM8 processor module"
      echo "— requires the STM8 plugin from github.com/d3v1l401/Ghidra-STM8)."
      exit 0
    fi
  fi
done

echo
echo "----------------------------------------------------------------------"
echo "Neither SWD nor SWIM produced a readable dump."
echo "  Possible causes:"
echo "    - Wiring error — double-check against README.md Wiring table"
echo "    - ST-Link firmware needs updating via STM32CubeProgrammer"
echo "    - Chip is readout-protected (RDP level 1+ on STM32, ROP on STM8)"
echo "    - MCU2 is a non-ST chip with a proprietary ISP interface"
echo
echo "Next step: fall back to RF sniffing via Crazyradio PA (see Phase 1 in README.md)."
echo "----------------------------------------------------------------------"
exit 1
