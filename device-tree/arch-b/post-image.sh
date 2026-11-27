#!/bin/bash
set -e

BINARIES_DIR="$1"
SCRIPT_DIR="$(dirname "$0")"

mkdir -p "${BINARIES_DIR}/rpi-firmware/overlays"

# Copy compiled overlay into rpi-firmware/overlays so genimage packages it into boot.vfat
if [ -f "${SCRIPT_DIR}/mpu9250-iio.dtbo" ]; then
    cp -f "${SCRIPT_DIR}/mpu9250-iio.dtbo" "${BINARIES_DIR}/rpi-firmware/overlays/"
elif command -v dtc >/dev/null 2>&1; then
    dtc -@ -I dts -O dtb -o "${BINARIES_DIR}/rpi-firmware/overlays/mpu9250-iio.dtbo" "${SCRIPT_DIR}/mpu9250-iio-overlay.dts"
fi
