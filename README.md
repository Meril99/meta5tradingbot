# telegram-mt5-bot

A C++17 copy trading bot that listens to a Telegram channel for trading signals and automatically forwards them to a MetaTrader 5 demo account via ZeroMQ. Designed to run as three systemd services on an Ubuntu 24.04 VPS.

## Architecture

```
┌─────────────────────┐
│    Telegram Cloud    │
│  (Signal Channel)    │
└─────────┬───────────┘
          │  TDLib C++ API
          ▼
┌─────────────────────┐     ZMQ PUSH/PULL     ┌─────────────────────┐
│   tdlib-listener    │──────── :5555 ────────▶│   signal-parser     │
│                     │                        │                     │
│ • TDLib auth        │                        │ • Regex parse       │
│ • Channel filter    │                        │ • Risk calc (lots)  │
│ • Raw text forward  │                        │ • Serialize signal  │
└─────────────────────┘                        └─────────┬───────────┘
                                                         │
                                               ZMQ PUSH/PULL :5556
                                                         │
                                    ┌────────────────────┼────────────────────┐
                                    ▼                                         ▼
                          ┌─────────────────────┐               ┌─────────────────────┐
                          │    mt5-bridge        │               │   TelegramBotEA     │
                          │    (C++ logger)      │               │   (MQL5 in MT5)     │
                          └──────────────────────┘               └─────────────────────┘
                                                                          │
                                                                          ▼
                                                                 ┌─────────────────┐
                                                                 │  MT5 Terminal    │
                                                                 │  (Wine on Linux) │
                                                                 │  PU Prime Demo   │
                                                                 └─────────────────┘
```

## Prerequisites

- Ubuntu 24.04 LTS (or similar)
- TDLib v1.8.0 (built from source by `install_deps.sh`)
- libzmq3 + cppzmq
- Wine 64-bit (for MT5)
- MetaTrader 5 (installed via Wine)
- Telegram API credentials (api_id + api_hash from https://my.telegram.org)
- PU Prime demo account

## 5-Minute Quickstart

```bash
# 1. Clone
git clone https://github.com/YOUR_USERNAME/telegram-mt5-bot.git
cd telegram-mt5-bot

# 2. Install dependencies (takes ~15 min for TDLib build)
sudo bash scripts/install_deps.sh

# 3. Configure
cp .env.example /etc/telegram-mt5-bot/.env
nano /etc/telegram-mt5-bot/.env   # fill in your values

# 4. Build
bash scripts/build.sh

# 5. First-time Telegram auth (interactive — enter code when prompted)
cd /var/lib/telegram-mt5-bot
source /etc/telegram-mt5-bot/.env && export TELEGRAM_API_ID TELEGRAM_API_HASH TELEGRAM_PHONE TELEGRAM_CHANNEL_ID ZMQ_LISTENER_PORT
/path/to/build/services/tdlib-listener/tdlib-listener
# Enter auth code, then Ctrl+C

# 6. Install and start services
sudo bash scripts/install_services.sh

# 7. Set up MT5 (see docs/SETUP_MT5.md)
```

For the full walkthrough, see [docs/SETUP_VPS.md](docs/SETUP_VPS.md).

## Signal Format Examples

The parser handles these formats:

```
# Format A
XAUUSD BUY @ 2310.50
SL: 2300.00
TP: 2330.00

# Format B
🟢 EURUSD BUY NOW
Entry: 1.08500
Stop Loss: 1.08000
Take Profit 1: 1.09000

# Format C
SELL GBPUSD
Price: 1.2650
SL 1.2700 TP 1.2580

# Format D
[SIGNAL] USDJPY SELL LIMIT 149.50
SL: 150.20 | TP: 148.00
```

Full documentation: [docs/SIGNAL_FORMATS.md](docs/SIGNAL_FORMATS.md)

## Monitoring

```bash
# Tail all service logs
journalctl -u tdlib-listener -u signal-parser -u mt5-bridge -f

# Or use the helper
bash scripts/logs.sh

# Check service status
systemctl status tdlib-listener signal-parser mt5-bridge

# View last received signal
cat /tmp/last_signal.json
```

## Adding New Signal Formats

1. Add a new `try_format_X()` method in `services/signal-parser/src/SignalParser.cpp`
2. Call it from `SignalParser::parse()` in the desired priority order
3. Add test cases in `tests/test_parser.cpp`
4. Document the format in `docs/SIGNAL_FORMATS.md`
5. Rebuild: `bash scripts/deploy.sh`

## Documentation

| Document | Description |
|----------|-------------|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | System design, data flow, design decisions |
| [SETUP_VPS.md](docs/SETUP_VPS.md) | Full VPS setup tutorial |
| [SETUP_MT5.md](docs/SETUP_MT5.md) | MT5 installation via Wine |
| [SETUP_TELEGRAM.md](docs/SETUP_TELEGRAM.md) | TDLib registration, finding channel IDs |
| [SIGNAL_FORMATS.md](docs/SIGNAL_FORMATS.md) | All supported signal formats |
| [SYSTEMD.md](docs/SYSTEMD.md) | Service management and log viewing |

## Disclaimer

This software is provided for **educational and testing purposes only**. Use it at your own risk. The authors are not responsible for any financial losses incurred through the use of this software. Always test on a demo account first. Trading involves significant risk of loss.
