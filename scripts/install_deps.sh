#!/usr/bin/env bash
# install_deps.sh — Install all system dependencies for telegram-mt5-bot.
# Targets Ubuntu 22.04+. Run as root or with sudo.
set -euo pipefail

echo "=== telegram-mt5-bot: Installing dependencies ==="

# ── 1. System packages ───────────────────────────────────────────────────────
echo "[1/4] Updating apt and installing system packages..."
apt-get update -y
apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    libssl-dev \
    zlib1g-dev \
    libzmq3-dev \
    libcurl4-openssl-dev \
    wine64 \
    wget \
    curl \
    pkg-config

# ── 2. Install cppzmq headers (for mt5-bridge C++ build) ────────────────────
CPPZMQ_DIR="/opt/cppzmq-build"

if [ -f /usr/local/include/zmq.hpp ]; then
    echo "[2/4] cppzmq headers already installed, skipping."
else
    echo "[2/4] Installing cppzmq headers..."
    rm -rf "${CPPZMQ_DIR}"
    git clone --depth 1 https://github.com/zeromq/cppzmq.git "${CPPZMQ_DIR}"
    cd "${CPPZMQ_DIR}"
    mkdir -p build && cd build
    cmake .. -DCPPZMQ_BUILD_TESTS=OFF
    cmake --build . --target install
    cd /
    rm -rf "${CPPZMQ_DIR}"
    echo "    cppzmq installed to /usr/local/include"
fi

# ── 3. Install Python dependencies ──────────────────────────────────────────
echo "[3/4] Installing Python packages..."
pip3 install telethon pyzmq python-dotenv

# ── 4. Create working directories ────────────────────────────────────────────
echo "[4/4] Creating working directories..."
mkdir -p /etc/telegram-mt5-bot
mkdir -p /var/lib/telegram-mt5-bot

echo ""
echo "=== All dependencies installed ==="
echo ""
echo "Next steps:"
echo "  1. Copy .env.example to /etc/telegram-mt5-bot/.env and fill in your values"
echo "  2. Run ./scripts/build.sh to compile mt5-bridge"
echo "  3. First-time Telegram auth:"
echo "     source /etc/telegram-mt5-bot/.env && export \$(grep -v '^#' /etc/telegram-mt5-bot/.env | xargs)"
echo "     python3 services/telegram-parser/telegram_parser.py"
echo "  4. Run ./scripts/install_services.sh to install systemd services"
