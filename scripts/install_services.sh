#!/usr/bin/env bash
# install_services.sh — Install services (Python telegram-parser + C++ mt5-bridge).
# Run as root or with sudo.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

echo "=== telegram-mt5-bot: Installing services ==="

# ── Stop old services if running ─────────────────────────────────────────────
echo "[1/5] Stopping old services (if any)..."
systemctl stop tdlib-listener.service 2>/dev/null || true
systemctl stop signal-parser.service 2>/dev/null || true
systemctl stop telegram-parser.service 2>/dev/null || true
systemctl stop mt5-bridge.service 2>/dev/null || true
systemctl disable tdlib-listener.service 2>/dev/null || true
systemctl disable signal-parser.service 2>/dev/null || true

# ── Install telegram-parser (Python) ────────────────────────────────────────
echo "[2/5] Installing telegram-parser..."
mkdir -p /opt/telegram-mt5-bot
cp "${PROJECT_DIR}/services/telegram-parser/telegram_parser.py" /opt/telegram-mt5-bot/
pip3 install -q -r "${PROJECT_DIR}/services/telegram-parser/requirements.txt"

# ── Install mt5-bridge (C++ binary) ─────────────────────────────────────────
echo "[3/5] Installing mt5-bridge binary..."
install -m 755 "${BUILD_DIR}/services/mt5-bridge/mt5-bridge" /usr/local/bin/

# ── Copy systemd units ──────────────────────────────────────────────────────
echo "[4/5] Installing systemd unit files..."
cp "${PROJECT_DIR}/services/telegram-parser/systemd/telegram-parser.service" /etc/systemd/system/
cp "${PROJECT_DIR}/services/mt5-bridge/systemd/mt5-bridge.service" /etc/systemd/system/

# ── Reload, enable, start ────────────────────────────────────────────────────
echo "[5/5] Reloading systemd and enabling services..."
systemctl daemon-reload
systemctl enable telegram-parser.service
systemctl enable mt5-bridge.service
systemctl start telegram-parser.service
systemctl start mt5-bridge.service

echo ""
echo "=== Services installed and started ==="
echo "Check status:"
echo "  systemctl status telegram-parser mt5-bridge"
echo "View logs:"
echo "  journalctl -u telegram-parser -u mt5-bridge -f"
