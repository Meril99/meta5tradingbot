# telegram-mt5-bot

A copy-trading bot that listens to a Telegram channel for scalping signals and automatically executes them on MetaTrader 5 via ZeroMQ. Designed for the RoyalFx Vip signal format on PU Prime.

## Architecture

```
┌─────────────────────┐
│    Telegram Cloud    │
│  (RoyalFx Vip)      │
└─────────┬───────────┘
          │  Telethon (Python)
          ▼
┌─────────────────────┐
│  telegram-parser    │     ZMQ PUSH/PULL     ┌─────────────────────┐
│  (Python)           │──────── :5556 ────────▶│   TelegramBotEA     │
│                     │                        │   (MQL5 in MT5)     │
│ • Telegram auth     │                        │                     │
│ • Channel monitor   │                        │ • Open 3 positions  │
│ • Scalp signal parse│                        │ • Break-even modify │
│ • Break-even detect │                        │ • Close-all         │
│ • Close-all detect  │                        │ • Trade events      │
└─────────────────────┘                        └─────────┬───────────┘
                                                         │
                                               ZMQ PUSH/PULL :5557
                                                         │
                                                         ▼
                                               ┌─────────────────────┐
                                               │    mt5-bridge        │
                                               │    (C++ service)     │
                                               │                     │
                                               │ • Trade journal CSV │
                                               │ • Telegram notifs   │
                                               └─────────────────────┘
```

## Signal Flow

1. **Entry** — "Gold sell now" / "Gold buy now" → opens 3 market orders (0.03, 0.02, 0.01 lots), no SL/TP
2. **Break-even** — "TP 1 ✅", "+35 pips ✅", "Don't forget your BE" → moves SL to entry on all open positions
3. **Close all** — "❌", "SL❌", "Close your last current positions" → closes everything

## Prerequisites

- Ubuntu 22.04+ (or similar Linux)
- Python 3.8+ with pip
- libzmq3 + cppzmq (for mt5-bridge)
- Wine 64-bit (for MT5)
- MetaTrader 5 (installed via Wine)
- Telegram API credentials (api_id + api_hash from https://my.telegram.org)
- PU Prime demo account

## Quickstart

```bash
# 1. Clone
git clone https://github.com/Meril99/Meta5Bot.git
cd Meta5Bot

# 2. Install system dependencies
sudo bash scripts/install_deps.sh

# 3. Configure
sudo cp .env.example /etc/telegram-mt5-bot/.env
sudo nano /etc/telegram-mt5-bot/.env   # fill in your values

# 4. Build mt5-bridge (C++)
bash scripts/build.sh

# 5. Install Python dependencies
pip3 install -r services/telegram-parser/requirements.txt

# 6. First-time Telegram auth (interactive)
source /etc/telegram-mt5-bot/.env && \
  export $(grep -v '^#' /etc/telegram-mt5-bot/.env | xargs) && \
  python3 services/telegram-parser/telegram_parser.py
# Enter phone + code when prompted, then Ctrl+C

# 7. Install and start services
sudo bash scripts/install_services.sh

# 8. Set up MT5 (see docs/SETUP_MT5.md)
```

## Configuration (.env)

| Variable | Description |
|----------|-------------|
| `TELEGRAM_API_ID` | From https://my.telegram.org |
| `TELEGRAM_API_HASH` | From https://my.telegram.org |
| `TELEGRAM_PHONE` | Your phone number (+352...) |
| `TELEGRAM_CHANNEL_ID` | Channel to monitor (negative number) |
| `TELEGRAM_BOT_TOKEN` | From @BotFather (for notifications) |
| `TELEGRAM_NOTIFY_CHAT_ID` | Your user ID (for notifications) |
| `FIXED_LOTS` | Lot sizes per trade: `0.03,0.02,0.01` |
| `TRADE_SYMBOL` | Broker symbol: `XAUUSD.s` |
| `ZMQ_PARSER_PORT` | Signal port (default 5556) |
| `ZMQ_EVENT_PORT` | Trade event port (default 5557) |

## Daily Run (Local Machine)

Open 2 terminals + MT5 before signals start (~9:30):

**Terminal 1 — Telegram Parser:**
```bash
cd /home/meril/Documents/Meta5Bot
source /etc/telegram-mt5-bot/.env && export $(grep -v '^#' /etc/telegram-mt5-bot/.env | xargs) && python3 services/telegram-parser/telegram_parser.py
```

**Terminal 2 — MT5 Bridge (journal + notifications):**
```bash
cd /home/meril/Documents/Meta5Bot
source /etc/telegram-mt5-bot/.env && export $(grep -v '^#' /etc/telegram-mt5-bot/.env | xargs) && ./build/services/mt5-bridge/mt5-bridge
```

**MT5 — Double-click "PU Prime MT5" on the desktop.** EA is already attached.

## Monitoring

```bash
# Tail all service logs
journalctl -u telegram-parser -u mt5-bridge -f

# Check status
systemctl status telegram-parser mt5-bridge

# View trade journal
cat /var/lib/telegram-mt5-bot/trade_journal.csv
```

## Disclaimer

This software is provided for **educational and testing purposes only**. Use it at your own risk. The authors are not responsible for any financial losses. Always test on a demo account first. Trading involves significant risk of loss.
