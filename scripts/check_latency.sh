#!/usr/bin/env bash
# check_latency.sh — Measure network latency to PU Prime trading servers
# and recommend VPS location for optimal execution.
# Run from your VPS: bash scripts/check_latency.sh
set -euo pipefail

# ── PU Prime known server hostnames and IPs ──────────────────────────────────
# These may change — update if ping fails. Check MT5 terminal for current IPs.
declare -A SERVERS=(
    ["PUPrime-Demo"]="demo.puprime.com"
    ["PUPrime-Live1"]="live1.puprime.com"
    ["PUPrime-Live2"]="live2.puprime.com"
    ["PUPrime-Live3"]="live3.puprime.com"
    ["PUPrime-Live4"]="live4.puprime.com"
    ["PUPrime-Live5"]="live5.puprime.com"
    ["PUPrime-Live6"]="live6.puprime.com"
    ["PUPrime-Live7"]="live7.puprime.com"
)

# Equinix data center reference IPs for triangulating your VPS location.
declare -A DATACENTERS=(
    ["London-LD4"]="198.32.160.1"
    ["Singapore-SG1"]="27.111.228.1"
    ["New-York-NY5"]="198.32.118.1"
    ["Tokyo-TY3"]="198.32.176.1"
    ["Frankfurt-FR5"]="80.81.192.1"
)

PING_COUNT=10
DIVIDER="────────────────────────────────────────────────────────"

echo "╔══════════════════════════════════════════════════════════╗"
echo "║     telegram-mt5-bot — Latency Check                    ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""
echo "VPS hostname : $(hostname)"
echo "VPS IP       : $(hostname -I 2>/dev/null | awk '{print $1}' || echo 'unknown')"
echo "Date         : $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo ""

# ── Helper: ping a host and extract avg latency ──────────────────────────────
ping_host() {
    local label="$1"
    local host="$2"
    local result

    # Try to resolve first.
    if ! host "$host" &>/dev/null && ! ping -c 1 -W 2 "$host" &>/dev/null; then
        printf "  %-22s %-30s  %s\n" "$label" "$host" "UNREACHABLE"
        return
    fi

    result=$(ping -c "$PING_COUNT" -W 3 "$host" 2>/dev/null | tail -1)

    if echo "$result" | grep -q "min/avg/max"; then
        # Extract min/avg/max/mdev
        local stats
        stats=$(echo "$result" | awk -F'=' '{print $2}' | tr '/' ' ')
        local min avg max
        min=$(echo "$stats" | awk '{print $1}')
        avg=$(echo "$stats" | awk '{print $2}')
        max=$(echo "$stats" | awk '{print $3}')

        # Color-code: green < 5ms, yellow 5-20ms, red > 20ms
        local rating
        if (( $(echo "$avg < 5" | bc -l 2>/dev/null || echo 0) )); then
            rating="EXCELLENT"
        elif (( $(echo "$avg < 10" | bc -l 2>/dev/null || echo 0) )); then
            rating="GOOD"
        elif (( $(echo "$avg < 20" | bc -l 2>/dev/null || echo 0) )); then
            rating="OK"
        else
            rating="HIGH"
        fi

        printf "  %-22s %-30s  avg=%6s ms  min=%6s  max=%6s  [%s]\n" \
               "$label" "$host" "$avg" "$min" "$max" "$rating"
    else
        printf "  %-22s %-30s  %s\n" "$label" "$host" "TIMEOUT"
    fi
}

# ── 1. Ping PU Prime servers ─────────────────────────────────────────────────
echo "$DIVIDER"
echo "  PU PRIME SERVERS (${PING_COUNT} pings each)"
echo "$DIVIDER"

for label in $(echo "${!SERVERS[@]}" | tr ' ' '\n' | sort); do
    ping_host "$label" "${SERVERS[$label]}"
done

echo ""

# ── 2. Ping Equinix data centers for reference ───────────────────────────────
echo "$DIVIDER"
echo "  DATA CENTER REFERENCE (triangulating your VPS location)"
echo "$DIVIDER"

for label in $(echo "${!DATACENTERS[@]}" | tr ' ' '\n' | sort); do
    ping_host "$label" "${DATACENTERS[$label]}"
done

echo ""

# ── 3. DNS resolution time for the target server ─────────────────────────────
echo "$DIVIDER"
echo "  DNS RESOLUTION"
echo "$DIVIDER"

TARGET_SERVER="live6.puprime.com"
DNS_START=$(date +%s%N)
if nslookup "$TARGET_SERVER" &>/dev/null; then
    DNS_END=$(date +%s%N)
    DNS_MS=$(( (DNS_END - DNS_START) / 1000000 ))
    RESOLVED_IP=$(nslookup "$TARGET_SERVER" 2>/dev/null | grep -A1 'Name:' | grep 'Address' | awk '{print $2}' | head -1)
    if [ -z "$RESOLVED_IP" ]; then
        RESOLVED_IP=$(dig +short "$TARGET_SERVER" 2>/dev/null | head -1 || echo "unknown")
    fi
    echo "  $TARGET_SERVER → $RESOLVED_IP (resolved in ${DNS_MS} ms)"
else
    echo "  $TARGET_SERVER → DNS resolution failed"
fi

echo ""

# ── 4. Traceroute to Server 6 (first 15 hops) ────────────────────────────────
echo "$DIVIDER"
echo "  TRACEROUTE to $TARGET_SERVER (max 15 hops)"
echo "$DIVIDER"

if command -v traceroute &>/dev/null; then
    traceroute -m 15 -w 2 "$TARGET_SERVER" 2>/dev/null || echo "  traceroute failed or blocked"
elif command -v tracepath &>/dev/null; then
    tracepath -m 15 "$TARGET_SERVER" 2>/dev/null || echo "  tracepath failed or blocked"
else
    echo "  (traceroute not installed — run: apt install traceroute)"
fi

echo ""

# ── 5. Recommendations ───────────────────────────────────────────────────────
echo "$DIVIDER"
echo "  RECOMMENDATIONS"
echo "$DIVIDER"
echo ""
echo "  Target for live trading:  < 5 ms to your broker server"
echo "  Acceptable:               5-10 ms"
echo "  Marginal:                 10-20 ms (noticeable slippage)"
echo "  Poor:                     > 20 ms (move your VPS)"
echo ""
echo "  Best VPS locations for PU Prime Server 6:"
echo "    1. Equinix LD4/LD5 London colocation (if budget allows)"
echo "    2. AWS eu-west-2 (London)"
echo "    3. Vultr/DigitalOcean London"
echo "    4. Hetzner Falkenstein (if LD ping is < 10 ms)"
echo ""
echo "  To re-check after moving VPS:"
echo "    bash scripts/check_latency.sh"
echo ""
echo "  To continuously monitor (every 60s for 1 hour):"
echo "    bash scripts/check_latency.sh --monitor"
echo ""

# ── 6. Optional: monitor mode ────────────────────────────────────────────────
if [[ "${1:-}" == "--monitor" ]]; then
    MONITOR_INTERVAL=60
    MONITOR_DURATION=3600
    MONITOR_LOG="/var/lib/telegram-mt5-bot/latency_log.csv"
    ELAPSED=0

    echo "$DIVIDER"
    echo "  MONITOR MODE — logging to $MONITOR_LOG"
    echo "  Interval: ${MONITOR_INTERVAL}s | Duration: ${MONITOR_DURATION}s"
    echo "  Press Ctrl+C to stop"
    echo "$DIVIDER"

    # Write CSV header if file doesn't exist.
    if [ ! -f "$MONITOR_LOG" ]; then
        echo "timestamp,server,min_ms,avg_ms,max_ms" > "$MONITOR_LOG"
    fi

    while [ "$ELAPSED" -lt "$MONITOR_DURATION" ]; do
        TIMESTAMP=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
        RESULT=$(ping -c 5 -W 3 "$TARGET_SERVER" 2>/dev/null | tail -1)

        if echo "$RESULT" | grep -q "min/avg/max"; then
            STATS=$(echo "$RESULT" | awk -F'=' '{print $2}' | tr '/' ',')
            MIN=$(echo "$STATS" | cut -d',' -f1 | tr -d ' ')
            AVG=$(echo "$STATS" | cut -d',' -f2 | tr -d ' ')
            MAX=$(echo "$STATS" | cut -d',' -f3 | tr -d ' ')
            echo "$TIMESTAMP,$TARGET_SERVER,$MIN,$AVG,$MAX" >> "$MONITOR_LOG"
            echo "  [$TIMESTAMP] avg=${AVG} ms  min=${MIN}  max=${MAX}"
        else
            echo "$TIMESTAMP,$TARGET_SERVER,,,timeout" >> "$MONITOR_LOG"
            echo "  [$TIMESTAMP] TIMEOUT"
        fi

        sleep "$MONITOR_INTERVAL"
        ELAPSED=$((ELAPSED + MONITOR_INTERVAL))
    done

    echo ""
    echo "  Monitor complete. Log: $MONITOR_LOG"
fi
