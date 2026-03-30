# MQL5 Expert Advisor — TelegramBotEA

MQL5 Expert Advisor that receives trading signals via ZeroMQ and places orders on a MetaTrader 5 demo account.

## Safety Guard

The EA **will not run on a live account**. On initialization, it checks `ACCOUNT_TRADE_MODE`. If the account is not in demo mode, it prints an error and removes itself.

## Installation

1. Install the mql5-zmq library (see `libs/README.md`)
2. Copy `TelegramBotEA.mq5` to your MT5 `MQL5/Experts/` directory
3. Open MetaEditor, compile the EA (F7)
4. Drag the EA onto any chart in MT5
5. Enable "Allow Algo Trading" and "Allow DLL imports" in EA settings
6. Click AutoTrading in the toolbar

## Input Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| ZMQ_HOST | 127.0.0.1 | ZeroMQ host address |
| ZMQ_PORT | 5556 | ZeroMQ port (must match signal-parser PUSH) |
| MAGIC_NUM | 123456 | Magic number to identify orders from this EA |

## Signal Format

Receives pipe-delimited strings:
```
SYMBOL|SIDE|ENTRY|SL|TP1|TP2|LOTS
```

- If `ENTRY == 0.0` or close to current price: places a market order
- Otherwise: places a limit order

## Logs

Check the **Experts** tab in MT5 for all EA log messages:
- `[EA][INFO]` — status messages
- `[EA][RECV]` — received signal
- `[EA][ORDER]` — order attempt
- `[EA][OK]` — order success with ticket number
- `[EA][FAIL]` — order failure with error code
