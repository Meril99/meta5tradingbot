# signal-parser

Receives raw Telegram messages via ZMQ PULL (port 5555), parses them into structured trading signals using regex, calculates lot size from risk parameters, and pushes serialized orders to port 5556 via ZMQ PUSH.

## How It Works

1. Connects ZMQ PULL to `tcp://127.0.0.1:5555` (listener's PUSH)
2. Binds ZMQ PUSH on `tcp://0.0.0.0:5556` (for mt5-bridge and EA)
3. For each message: tries four signal formats (A, B, C, D) in order
4. On successful parse: calculates lot size from risk parameters
5. Serializes as pipe-delimited string and pushes downstream

## Signal Formats

See [docs/SIGNAL_FORMATS.md](../../docs/SIGNAL_FORMATS.md) for detailed format documentation.

## Risk Management

Lot size is calculated as:
```
risk_amount = ACCOUNT_BALANCE * RISK_PERCENT / 100
sl_pips     = |entry - sl| / pip_size(symbol)
lots        = risk_amount / (sl_pips * pip_value(symbol))
lots        = clamp(lots, MIN_LOTS, MAX_LOTS)
```

## Environment Variables

| Variable | Description |
|----------|-------------|
| `ZMQ_LISTENER_PORT` | ZMQ PULL connect port (default: 5555) |
| `ZMQ_PARSER_PORT` | ZMQ PUSH bind port (default: 5556) |
| `ACCOUNT_BALANCE` | Account balance in USD |
| `RISK_PERCENT` | Risk per trade (%) |
| `MIN_LOTS` | Minimum lot size |
| `MAX_LOTS` | Maximum lot size |
