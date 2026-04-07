#!/usr/bin/env python3
"""telegram-parser — Replaces tdlib-listener + signal-parser.
Listens to a Telegram channel via Telethon, parses scalp signals,
and pushes them to ZMQ for the MT5 EA."""

import asyncio
import os
import re
import signal
import sys

from telethon import TelegramClient, events
import zmq

# ── Config from environment ──────────────────────────────────────────────────
API_ID    = int(os.environ["TELEGRAM_API_ID"])
API_HASH  = os.environ["TELEGRAM_API_HASH"]
PHONE     = os.environ["TELEGRAM_PHONE"]
CHANNEL   = int(os.environ["TELEGRAM_CHANNEL_ID"])
ZMQ_PORT  = os.environ.get("ZMQ_PARSER_PORT", "5556")
SYMBOL    = os.environ.get("TRADE_SYMBOL", "XAUUSD.s")
LOTS      = [float(x) for x in os.environ.get("FIXED_LOTS", "0.03,0.02,0.01").split(",")]
SESSION   = os.environ.get("TELETHON_SESSION", "telegram_session")

# ── ZMQ PUSH socket (EA and mt5-bridge PULL from here) ───────────────────────
ctx  = zmq.Context()
push = ctx.socket(zmq.PUSH)
push.bind(f"tcp://0.0.0.0:{ZMQ_PORT}")
print(f"[ZMQ] PUSH bound on tcp://0.0.0.0:{ZMQ_PORT}", flush=True)

# ── Regex patterns ───────────────────────────────────────────────────────────

# Entry: "Gold sell now", "Gold buy now" (with optional emojis / suffixes)
RE_ENTRY = re.compile(r"gold\s+(buy|sell)\s+now", re.I)

# Break-even triggers
RE_BE = [
    re.compile(r"TP\s*1\s*\u2705", re.I),           # TP 1 ✅
    re.compile(r"\+\d+\s*pips?\s*\u2705", re.I),     # +35 pips ✅
    re.compile(r"forget\s+your\s+BE", re.I),          # Don't forget your BE
    re.compile(r"BE\s+hit", re.I),                    # BE hit ✅ / BE HIT 0 pips ✅
    re.compile(r"from\s+the\s+zone\s+BE", re.I),     # +60 pips from the zone BE ...
]

# Close-all triggers
RE_CLOSE_TEXT = [
    re.compile(r"close\s+(it\s+at|your\s+last)", re.I),  # Close it at ... / Close your last ...
    re.compile(r"we\s+cut\s+the\s+trade", re.I),         # We cut the trade in positive
    re.compile(r"SL\s*\u274C", re.I),                     # SL❌
]


def parse(text: str):
    """Return (signal_type, side) or None."""
    stripped = text.strip()

    # ── Close-all (highest priority) ─────────────────────────────────────
    if stripped == "\u274C":                        # standalone ❌
        return ("CLOSEALL", None)
    for rx in RE_CLOSE_TEXT:
        if rx.search(text):
            return ("CLOSEALL", None)

    # ── Break-even ───────────────────────────────────────────────────────
    for rx in RE_BE:
        if rx.search(text):
            return ("BREAKEVEN", None)

    # ── Scalp entry ──────────────────────────────────────────────────────
    m = RE_ENTRY.search(text)
    if m:
        return ("ENTRY", m.group(1).upper())

    return None


def zmq_send(msg: str):
    push.send_string(msg)
    print(f"[SEND] {msg}", flush=True)


async def main():
    print(f"[CONFIG] Symbol={SYMBOL}  Lots={LOTS}  Channel={CHANNEL}", flush=True)

    client = TelegramClient(SESSION, API_ID, API_HASH)
    await client.start(phone=PHONE)
    me = await client.get_me()
    print(f"[TELEGRAM] Logged in as {me.first_name} (id={me.id})", flush=True)

    @client.on(events.NewMessage(chats=CHANNEL))
    async def on_message(event):
        text = event.raw_text
        if not text:
            return

        print(f"[MSG] {text[:120]}", flush=True)

        result = parse(text)
        if result is None:
            print("[SKIP] Not a signal", flush=True)
            return

        sig_type, side = result

        if sig_type == "CLOSEALL":
            zmq_send(f"CLOSEALL|{SYMBOL}")

        elif sig_type == "BREAKEVEN":
            zmq_send(f"BREAKEVEN|{SYMBOL}")

        elif sig_type == "ENTRY":
            for lot in LOTS:
                zmq_send(f"{SYMBOL}|{side}|0.00000|0.00000|0.00000|0.00000|{lot:.2f}")

    # Graceful shutdown on SIGINT / SIGTERM
    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.ensure_future(client.disconnect()))

    print("[READY] Waiting for signals...", flush=True)
    await client.run_until_disconnected()
    print("[DONE] Disconnected.", flush=True)


if __name__ == "__main__":
    asyncio.run(main())
