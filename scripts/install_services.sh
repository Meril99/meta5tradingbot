#!/usr/bin/env bash
# install_services.sh — Install binaries and systemd service units.
# Run as root or with sudo.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "=== telegram-mt5-bot: Installing services ==="

# ── Copy binaries ─────────────────────────────────────────────────────────────
echo "[1/4] Installing binaries to /usr/local/bin/..."
install -m 755 "${BUILD_DIR}/services/tdlib-listener/tdlib-listener" /usr/local/bin/
install -m 755 "${BUILD_DIR}/services/signal-parser/signal-parser"   /usr/local/bin/
install -m 755 "${BUILD_DIR}/services/mt5-bridge/mt5-bridge"         /usr/local/bin/

# ── Copy systemd units ───────────────────────────────────────────────────────
echo "[2/4] Installing systemd unit files..."
cp "${PROJECT_DIR}/services/tdlib-listener/systemd/tdlib-listener.service" /etc/systemd/system/
cp "${PROJECT_DIR}/services/signal-parser/systemd/signal-parser.service"   /etc/systemd/system/
cp "${PROJECT_DIR}/services/mt5-bridge/systemd/mt5-bridge.service"         /etc/systemd/system/

# ── Reload and enable ────────────────────────────────────────────────────────
echo "[3/4] Reloading systemd and enabling services..."
systemctl daemon-reload
systemctl enable tdlib-listener.service
systemctl enable signal-parser.service
systemctl enable mt5-bridge.service

# ── Start ─────────────────────────────────────────────────────────────────────
echo "[4/4] Starting services..."
systemctl start tdlib-listener.service
systemctl start signal-parser.service
systemctl start mt5-bridge.service

echo ""
echo "=== Services installed and started ==="
echo "Check status:"
echo "  systemctl status tdlib-listener signal-parser mt5-bridge"
echo "View logs:"
echo "  journalctl -u tdlib-listener -u signal-parser -u mt5-bridge -f"
