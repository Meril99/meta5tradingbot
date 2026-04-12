# =============================================================================
# Multi-stage Dockerfile for the MT5 Bridge service (C++17)
# Receives trade events from EA via ZMQ, logs to CSV, sends Telegram alerts
#
# Build:  docker build -t mt5-bridge -f services/mt5-bridge/Dockerfile .
# Run:    docker run --env-file /etc/telegram-mt5-bot/.env mt5-bridge
# =============================================================================

# ── Stage 1: C++ Builder ────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        pkg-config \
        libzmq3-dev \
        libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install cppzmq headers
RUN git clone --depth 1 https://github.com/zeromq/cppzmq.git /tmp/cppzmq \
    && cd /tmp/cppzmq && mkdir build && cd build \
    && cmake .. -DCPPZMQ_BUILD_TESTS=OFF \
    && cmake --build . --target install \
    && rm -rf /tmp/cppzmq

WORKDIR /build
COPY CMakeLists.txt ./
COPY services/mt5-bridge/ ./services/mt5-bridge/

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . -j "$(nproc)"

# ── Stage 2: Minimal Runtime ────────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

LABEL maintainer="meril" \
      component="mt5-bridge" \
      description="MT5 Bridge — trade event receiver, CSV journal, Telegram notifier"

RUN apt-get update && apt-get install -y --no-install-recommends \
        libzmq5 \
        libcurl4 \
        tini \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --shell /bin/bash bridge

COPY --from=builder /build/build/services/mt5-bridge/mt5-bridge /usr/local/bin/mt5-bridge

# Journal storage (mount as volume)
RUN mkdir -p /data && chown bridge:bridge /data
VOLUME ["/data"]
ENV TRADE_JOURNAL_PATH=/data/trade_journal.csv

USER bridge

ENTRYPOINT ["tini", "--"]
CMD ["mt5-bridge"]
