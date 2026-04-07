#!/usr/bin/env bash
# build.sh — Build mt5-bridge C++ service using CMake.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "=== telegram-mt5-bot: Building project ==="
echo "    Source:  ${PROJECT_DIR}"
echo "    Build:   ${BUILD_DIR}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${PROJECT_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build . -j "$(nproc)"

echo ""
echo "=== Build complete ==="
echo "Binary: ${BUILD_DIR}/services/mt5-bridge/mt5-bridge"
echo ""
echo "Next: run ./scripts/install_services.sh to install services."
