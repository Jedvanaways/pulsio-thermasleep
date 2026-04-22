#!/usr/bin/env bash
# One-shot install of Bastille Research's nRF24 sniffer tooling on macOS.
# Flashes a Crazyradio PA with the nrf-research-firmware and installs the Python tools.
#
# Usage:
#   bash tools/install-bastille-macos.sh
#
# Requires:
#   - Homebrew
#   - A Crazyradio PA plugged into the Mac
#   - Python 3.9+
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> Checking prerequisites"
command -v brew >/dev/null || { echo "Install Homebrew first: https://brew.sh"; exit 1; }
command -v python3 >/dev/null || { brew install python; }

echo "==> Installing libusb (needed for Crazyradio USB access)"
brew install libusb sdcc

echo "==> Cloning nrf-research-firmware (if not already present)"
if [ ! -d nrf-research-firmware ]; then
  git clone https://github.com/BastilleResearch/nrf-research-firmware.git
fi
cd nrf-research-firmware

echo "==> Setting up Python venv"
python3 -m venv .venv
# shellcheck disable=SC1091
source .venv/bin/activate
pip install --upgrade pip
pip install pyusb

echo "==> Building firmware"
make

echo "==> Flashing Crazyradio PA"
echo "    Make sure a Crazyradio PA is plugged in. Press Enter to continue."
read -r
sudo make install

echo
echo "==> Done. Next steps:"
echo "    cd nrf-research-firmware/tools"
echo "    source ../.venv/bin/activate"
echo "    ./nrf24-scanner.py -c 2-83"
echo
echo "    Hold the ThermaSleep remote within 30cm of the Crazyradio,"
echo "    press a button, and watch for 'CH:/A:/P:' lines in the scanner output."
