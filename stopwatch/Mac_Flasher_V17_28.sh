#!/usr/bin/env bash
set -euo pipefail

# V17.28 M5Stack StopWatch C152 Mac flasher.
# This flashes ONLY bootloader, partition table, and app, preserving NVS
# preferences at 0x9000 and the LittleFS area (do not use merged.bin here).
cd "$(dirname "$0")"
PREFIX="StopWatchNative31V16.ino"
for name in "$PREFIX.bootloader.bin" "$PREFIX.partitions.bin" "$PREFIX.bin"; do
  if [[ ! -f "$name" ]]; then
    echo "Missing $name. Use this script from the GitHub Actions firmware artifact folder." >&2
    exit 1
  fi
done
if [[ -f SHA256SUMS.txt ]]; then
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 -c SHA256SUMS.txt
  fi
fi
if [[ $# -ne 1 ]]; then
  echo "Usage: bash Mac_Flasher.sh /dev/cu.usbmodemXXXX"
  echo "Available ports:"
  ls /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null || true
  echo "Hold power on the StopWatch ~2 seconds to enter download mode, then run with its USB port."
  exit 2
fi
if ! python3 -m esptool version >/dev/null 2>&1; then
  echo "Missing Python esptool. Install with: python3 -m pip install --user esptool" >&2
  exit 2
fi
PORT="$1"
echo "Flashing StopWatch V17.28, ESP32-S3, on $PORT"
echo "This preserves the NVS settings partition and existing LittleFS data."
python3 -m esptool --chip esp32s3 --port "$PORT" --baud 460800 write_flash \
  0x0 "$PREFIX.bootloader.bin" \
  0x8000 "$PREFIX.partitions.bin" \
  0x10000 "$PREFIX.bin"
echo "Flashed. Disconnect/reconnect and power on the watch."
