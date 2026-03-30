# mql5-zmq Library Installation

The TelegramBotEA requires the **mql-zmq** library by dingmaotu for ZeroMQ communication from MQL5.

## Download

Repository: https://github.com/dingmaotu/mql-zmq

```bash
git clone https://github.com/dingmaotu/mql-zmq.git /tmp/mql-zmq
```

## Install into MT5

Set your MT5 directory (adjust for your Wine prefix):
```bash
MT5DIR="$HOME/.wine_mt5/drive_c/Program Files/MetaTrader 5"
```

### Copy Include Headers
```bash
cp -r /tmp/mql-zmq/Include/Zmq  "$MT5DIR/MQL5/Include/"
cp -r /tmp/mql-zmq/Include/Mql  "$MT5DIR/MQL5/Include/"
```

### Copy DLL Libraries
```bash
cp /tmp/mql-zmq/Library/MT5/libzmq.dll     "$MT5DIR/MQL5/Libraries/"
cp /tmp/mql-zmq/Library/MT5/libsodium.dll   "$MT5DIR/MQL5/Libraries/"
```

## Verify

After copying, your MT5 directory should have:

```
MQL5/
├── Include/
│   ├── Zmq/
│   │   └── Zmq.mqh    (+ other files)
│   └── Mql/
│       └── ...
├── Libraries/
│   ├── libzmq.dll
│   └── libsodium.dll
└── Experts/
    └── TelegramBotEA.mq5
```

## Compile

1. Open MetaEditor in MT5 (F4)
2. Open `Experts/TelegramBotEA.mq5`
3. Press F7 to compile
4. Check the output panel for errors — there should be none

## Important Notes

- **DLL imports must be enabled** in the EA settings (Common tab → "Allow DLL imports")
- The DLLs are 64-bit — ensure you're using `terminal64.exe`
- If compilation fails with "Zmq.mqh not found", check the Include directory paths
