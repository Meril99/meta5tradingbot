#!/usr/bin/env bash
# install_deps.sh — Install all system dependencies for telegram-mt5-bot.
# Targets Ubuntu 24.04. Run as root or with sudo.
set -euo pipefail

echo "=== telegram-mt5-bot: Installing dependencies ==="

# ── 1. System packages ───────────────────────────────────────────────────────
echo "[1/5] Updating apt and installing system packages..."
apt-get update -y
apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    zlib1g-dev \
    libzmq3-dev \
    libcurl4-openssl-dev \
    wine64 \
    wget \
    curl \
    pkg-config \
    gperf \
    traceroute \
    dnsutils \
    bc \
    php-cli   # TDLib build dependency

# ── 2. Build TDLib from source ───────────────────────────────────────────────
TDLIB_TAG="v1.8.0"
TDLIB_DIR="/opt/tdlib-build"

if [ -f /usr/local/lib/libtdclient.so ] || [ -f /usr/local/lib/libtdclient.a ]; then
    echo "[2/5] TDLib already installed, skipping build."
else
    echo "[2/5] Cloning and building TDLib ${TDLIB_TAG}..."
    rm -rf "${TDLIB_DIR}"
    git clone --depth 1 --branch "${TDLIB_TAG}" \
        https://github.com/tdlib/td.git "${TDLIB_DIR}"

    cd "${TDLIB_DIR}"
    mkdir -p build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local
    cmake --build . --target install -j "$(nproc)"
    cd /
    rm -rf "${TDLIB_DIR}"
    ldconfig
    echo "    TDLib installed to /usr/local"
fi

# ── 3. Install cppzmq headers ────────────────────────────────────────────────
CPPZMQ_DIR="/opt/cppzmq-build"

if [ -f /usr/local/include/zmq.hpp ]; then
    echo "[3/5] cppzmq headers already installed, skipping."
else
    echo "[3/5] Installing cppzmq headers..."
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

# ── 4. Create working directories ────────────────────────────────────────────
echo "[4/5] Creating working directories..."
mkdir -p /etc/telegram-mt5-bot
mkdir -p /var/lib/telegram-mt5-bot

# ── 5. Done ──────────────────────────────────────────────────────────────────
echo "[5/5] All dependencies installed successfully!"
echo ""
echo "Next steps:"
echo "  1. Copy .env.example to /etc/telegram-mt5-bot/.env and fill in your values"
echo "  2. Run ./scripts/build.sh to compile the project"
echo "  3. Run ./scripts/install_services.sh to install systemd services"
