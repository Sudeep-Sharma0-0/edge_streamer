#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

echo -e "=== Edge Streamer build started ===\n"

# 1. Configure and Build
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Export compile commands during CMake configuration
cmake ..
cmake --build .

echo -e "\n=== Edge Streamer build completed successfully ==="

echo -e "\n=== Generating compile_commands.json for local LSP ==="

rm -f "${PROJECT_ROOT}/compile_commands.json"

cp "${BUILD_DIR}/compile_commands.json" "${PROJECT_ROOT}/compile_commands.json"

# Translate Raspberry Pi paths -> Fedora paths & Sysroot
sed -i \
  -e 's|/home/cloud9/sudip/edge_streamer|/home/sudip/pi_code/edge_streamer|g' \
  -e 's|-I/usr/local/include|-I/home/sudip/pi_sysroot/usr/local/include|g' \
  -e 's|-I/usr/include|-I/home/sudip/pi_sysroot/usr/include|g' \
  -e 's|-isystem /usr/include|-isystem /home/sudip/pi_sysroot/usr/include|g' \
  -e 's|-isystem /usr/local/include|-isystem /home/sudip/pi_sysroot/usr/local/include|g' \
  -e 's|-isystem /usr/lib/aarch64-linux-gnu|-isystem /home/sudip/pi_sysroot/usr/lib/aarch64-linux-gnu|g' \
  -e 's|-I/usr/lib/aarch64-linux-gnu|-I/home/sudip/pi_sysroot/usr/lib/aarch64-linux-gnu|g' \
  "${PROJECT_ROOT}/compile_commands.json"

echo -e "✓ compile_commands.json synced and patched for Fedora sysroot."
