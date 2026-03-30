#!/usr/bin/env bash
# deploy.sh — Full deploy: build + install services.
# Run as root or with sudo.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== telegram-mt5-bot: Full deploy ==="
echo ""

# Stop existing services (ignore errors if not running)
echo "[1/3] Stopping existing services (if running)..."
systemctl stop tdlib-listener.service  2>/dev/null || true
systemctl stop signal-parser.service   2>/dev/null || true
systemctl stop mt5-bridge.service      2>/dev/null || true

echo "[2/3] Building..."
bash "${SCRIPT_DIR}/build.sh"

echo "[3/3] Installing services..."
bash "${SCRIPT_DIR}/install_services.sh"

echo ""
echo "=== Deploy complete ==="
