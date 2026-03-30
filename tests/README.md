# Tests

## Unit Tests for SignalParser

`test_parser.cpp` tests the signal parser against all four formats plus edge cases.

### Compile and Run

```bash
cd tests
g++ -std=c++17 test_parser.cpp ../services/signal-parser/src/SignalParser.cpp -o test_parser
./test_parser
```

### What's Tested

- Format A: standard, SELL variant, no-space-around-@
- Format B: emoji prefix, two TPs, single TP, SELL variant
- Format C: SELL GBPUSD, BUY USDJPY, XAUUSD
- Format D: SELL LIMIT, BUY LIMIT, BUY STOP
- Edge cases: lowercase, missing TP, garbage text, empty string, emoji-only
- Serialization round-trip
- Lot size calculation accuracy
- Instruments: XAUUSD, EURUSD, GBPUSD, USDJPY, GBPJPY, AUDUSD, NASDAQ, USOIL, US30

### Sample Signals

`sample_signals.txt` contains 23 example signals used for manual and automated testing.
