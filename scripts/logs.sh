#!/usr/bin/env bash
# logs.sh — Tail logs from all three telegram-mt5-bot services.
set -euo pipefail

echo "=== Tailing logs for all telegram-mt5-bot services ==="
echo "    Press Ctrl+C to stop."
echo ""

journalctl -u tdlib-listener -u signal-parser -u mt5-bridge -f --no-pager
