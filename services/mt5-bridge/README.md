# mt5-bridge

Receives parsed trading signals (port 5556) and trade events from the EA (port 5557). Logs signals, sends Telegram notifications for trade events, and records all trades to a CSV journal.

## How It Works

1. ZMQ PULL connects to `tcp://127.0.0.1:5556` for parsed signals
2. ZMQ PULL binds on `tcp://0.0.0.0:5557` for trade events from EA
3. Uses `zmq_poll` to watch both sockets
4. Signals: deserialized, logged, written to `/tmp/last_signal.json`
5. Trade events: logged, recorded to CSV, sent as Telegram notification

## Trade Notifications

When `TELEGRAM_BOT_TOKEN` and `TELEGRAM_NOTIFY_CHAT_ID` are set, sends formatted messages for: trade opened, closed, SL hit, TP hit, break even.

## Trade Journal

Appends every event to a CSV file with columns: event type, symbol, side, lots, prices, P&L, pips, risk/reward ratio, duration, spread, and more. Default path: `/var/lib/telegram-mt5-bot/trade_journal.csv`.

## Environment Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `ZMQ_PARSER_PORT` | Yes | Signal PULL connect port (default: 5556) |
| `ZMQ_EVENT_PORT` | Yes | Event PULL bind port (default: 5557) |
| `TELEGRAM_BOT_TOKEN` | No | Bot token for notifications |
| `TELEGRAM_NOTIFY_CHAT_ID` | No | Chat ID for notifications |
| `TRADE_JOURNAL_PATH` | No | CSV path (default: `/var/lib/telegram-mt5-bot/trade_journal.csv`) |
