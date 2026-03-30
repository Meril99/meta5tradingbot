# tdlib-listener

Connects to Telegram using TDLib's C++ API, monitors a specific channel, and forwards raw message text to the signal-parser via ZeroMQ PUSH on port 5555.

## How It Works

1. Authenticates with Telegram using `api_id`, `api_hash`, and phone number from `.env`
2. Stores session in `tdlib_session/` directory for automatic re-auth on restart
3. Filters incoming messages by `TELEGRAM_CHANNEL_ID`
4. Sends raw UTF-8 message text over ZMQ PUSH socket
5. Writes a heartbeat file every 30s for systemd watchdog

## First-Time Auth

The first run requires interactive input (Telegram sends a verification code). Run manually:

```bash
cd /var/lib/telegram-mt5-bot
source /etc/telegram-mt5-bot/.env
export TELEGRAM_API_ID TELEGRAM_API_HASH TELEGRAM_PHONE TELEGRAM_CHANNEL_ID ZMQ_LISTENER_PORT
./tdlib-listener
```

After entering the code, the session is saved and all future starts are automatic.

## Environment Variables

| Variable | Description |
|----------|-------------|
| `TELEGRAM_API_ID` | Telegram API app ID |
| `TELEGRAM_API_HASH` | Telegram API app hash |
| `TELEGRAM_PHONE` | Phone number with country code |
| `TELEGRAM_CHANNEL_ID` | Channel to monitor (negative number) |
| `ZMQ_LISTENER_PORT` | ZMQ PUSH bind port (default: 5555) |
