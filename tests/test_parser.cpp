// test_parser.cpp — Unit tests for SignalParser.
// Compiles standalone: g++ -std=c++17 test_parser.cpp ../services/signal-parser/src/SignalParser.cpp -o test_parser

#include "../services/signal-parser/src/SignalParser.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

// Test parameters: 10,000 USD balance, 1% risk, 0.01–1.0 lots.
static const double BALANCE  = 10000.0;
static const double RISK_PCT = 1.0;
static const double MIN_LOTS = 0.01;
static const double MAX_LOTS = 1.0;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(name, cond) do { \
    if (cond) { \
        std::cout << "[PASS] " << (name) << "\n"; \
        ++g_pass; \
    } else { \
        std::cout << "[FAIL] " << (name) << "\n"; \
        ++g_fail; \
    } \
} while(0)

static bool approx(double a, double b, double eps = 0.001) {
    return std::abs(a - b) < eps;
}

int main() {
    SignalParser parser(BALANCE, RISK_PCT, MIN_LOTS, MAX_LOTS);
    parser.set_trade_symbol("XAUUSD.s");

    std::cout << "=== SignalParser Unit Tests ===\n\n";

    // ── Scalp entry tests ───────────────────────────────────────────────

    // T01: "Gold sell now" with red circle emoji
    {
        std::string text = "Gold sell now \xF0\x9F\x94\xB4";
        Signal sig = parser.parse(text);
        CHECK("T01 Scalp sell valid",        sig.valid);
        CHECK("T01 type ENTRY",             sig.type == SignalType::ENTRY);
        CHECK("T01 symbol XAUUSD.s",        sig.symbol == "XAUUSD.s");
        CHECK("T01 side SELL",              sig.side == Signal::Side::SELL);
        CHECK("T01 entry 0 (market)",       approx(sig.entry, 0.0));
        CHECK("T01 sl 0 (none)",            approx(sig.sl, 0.0));
        CHECK("T01 tp1 0 (none)",           approx(sig.tp1, 0.0));
    }

    // T02: "Gold buy now" with green circle emoji
    {
        std::string text = "Gold buy now \xF0\x9F\x9F\xA2";
        Signal sig = parser.parse(text);
        CHECK("T02 Scalp buy valid",         sig.valid);
        CHECK("T02 side BUY",               sig.side == Signal::Side::BUY);
        CHECK("T02 symbol XAUUSD.s",        sig.symbol == "XAUUSD.s");
    }

    // T03: "Gold Buy now" with blue diamond + "| xauusd" suffix
    {
        std::string text = "Gold Buy now \xF0\x9F\x94\xB7 | xauusd\nScalping \xE2\x9C\x85\nUse a good MM (money management)";
        Signal sig = parser.parse(text);
        CHECK("T03 Scalp buy with suffix",   sig.valid);
        CHECK("T03 side BUY",               sig.side == Signal::Side::BUY);
        CHECK("T03 symbol XAUUSD.s",        sig.symbol == "XAUUSD.s");
    }

    // T04: Case variations
    {
        Signal sig = parser.parse("GOLD SELL NOW");
        CHECK("T04 Uppercase scalp",        sig.valid);
        CHECK("T04 side SELL",              sig.side == Signal::Side::SELL);
    }

    // ── Break-even tests ────────────────────────────────────────────────

    // T05: "TP 1 ✅"
    {
        std::string text = "TP 1 \xE2\x9C\x85";
        Signal sig = parser.parse(text);
        CHECK("T05 BE: TP 1 valid",         sig.valid);
        CHECK("T05 type BREAKEVEN",         sig.type == SignalType::BREAKEVEN);
        CHECK("T05 symbol XAUUSD.s",        sig.symbol == "XAUUSD.s");
    }

    // T06: "+35 pips ✅"
    {
        std::string text = "+35 pips \xE2\x9C\x85";
        Signal sig = parser.parse(text);
        CHECK("T06 BE: +35 pips valid",     sig.valid);
        CHECK("T06 type BREAKEVEN",         sig.type == SignalType::BREAKEVEN);
    }

    // T07: "Don't forget your BE"
    {
        Signal sig = parser.parse("Don't forget your BE");
        CHECK("T07 BE: forget your BE",     sig.valid);
        CHECK("T07 type BREAKEVEN",         sig.type == SignalType::BREAKEVEN);
    }

    // T08: "BE hit ✅"
    {
        std::string text = "BE hit \xE2\x9C\x85";
        Signal sig = parser.parse(text);
        CHECK("T08 BE: hit valid",          sig.valid);
        CHECK("T08 type BREAKEVEN",         sig.type == SignalType::BREAKEVEN);
    }

    // T09: "BE HIT 0 pips ✅"
    {
        std::string text = "BE HIT 0 pips \xE2\x9C\x85";
        Signal sig = parser.parse(text);
        CHECK("T09 BE: HIT 0 pips",        sig.valid);
        CHECK("T09 type BREAKEVEN",         sig.type == SignalType::BREAKEVEN);
    }

    // T10: "+60 pips from the zone BE or take profits ✅ scalp"
    {
        std::string text = "+60 pips from the zone BE or take profits \xE2\x9C\x85 scalp";
        Signal sig = parser.parse(text);
        CHECK("T10 BE: zone BE valid",      sig.valid);
        CHECK("T10 type BREAKEVEN",         sig.type == SignalType::BREAKEVEN);
    }

    // ── Close-all tests ───────────────────────────────────────────────

    // T11: "Close it at the entry point ✅ 0 pips"
    {
        std::string text = "Close it at the entry point \xE2\x9C\x85 0 pips";
        Signal sig = parser.parse(text);
        CHECK("T11 Close entry point",      sig.valid);
        CHECK("T11 type CLOSE_ALL",         sig.type == SignalType::CLOSE_ALL);
        CHECK("T11 symbol XAUUSD.s",        sig.symbol == "XAUUSD.s");
    }

    // T12: standalone "❌"
    {
        std::string text = "\xE2\x9D\x8C";
        Signal sig = parser.parse(text);
        CHECK("T12 Close: standalone X",    sig.valid);
        CHECK("T12 type CLOSE_ALL",         sig.type == SignalType::CLOSE_ALL);
    }

    // T13: "SL❌"
    {
        std::string text = "SL\xE2\x9D\x8C";
        Signal sig = parser.parse(text);
        CHECK("T13 Close: SL X",           sig.valid);
        CHECK("T13 type CLOSE_ALL",         sig.type == SignalType::CLOSE_ALL);
    }

    // T14: "Close your last current positions"
    {
        Signal sig = parser.parse("Close your last current positions");
        CHECK("T14 Close last positions",    sig.valid);
        CHECK("T14 type CLOSE_ALL",         sig.type == SignalType::CLOSE_ALL);
    }

    // T15: "We cut the trade in positive"
    {
        Signal sig = parser.parse("We cut the trade in positive");
        CHECK("T15 Close: cut trade",       sig.valid);
        CHECK("T15 type CLOSE_ALL",         sig.type == SignalType::CLOSE_ALL);
    }

    // ── Serialization tests ─────────────────────────────────────────────

    // T16: Entry serialization
    {
        Signal sig = parser.parse("Gold sell now");
        sig.lots = 0.03;
        std::string s = sig.serialize();
        CHECK("T16 Serialize entry",        s.find("XAUUSD.s|SELL|") == 0);
        CHECK("T16 Serialize has 6 pipes",  std::count(s.begin(), s.end(), '|') == 6);
    }

    // T17: Breakeven serialization
    {
        std::string text = "TP 1 \xE2\x9C\x85";
        Signal sig = parser.parse(text);
        std::string s = sig.serialize();
        CHECK("T17 Serialize BE",           s == "BREAKEVEN|XAUUSD.s");
    }

    // T18: Close-all serialization
    {
        Signal sig = parser.parse("We cut the trade in positive");
        std::string s = sig.serialize();
        CHECK("T18 Serialize CLOSEALL",     s == "CLOSEALL|XAUUSD.s");
    }

    // ── Legacy format tests (still work) ────────────────────────────────

    // T19: Format A
    {
        std::string text = "EURUSD BUY @ 1.08500\nSL: 1.08000\nTP: 1.09000";
        Signal sig = parser.parse(text);
        CHECK("T19 Format A valid",         sig.valid);
        CHECK("T19 symbol EURUSD",          sig.symbol == "EURUSD");
        CHECK("T19 side BUY",               sig.side == Signal::Side::BUY);
        CHECK("T19 entry 1.085",            approx(sig.entry, 1.085));
    }

    // T20: Format B
    {
        std::string text =
            "\xF0\x9F\x9F\xA2 EURUSD BUY NOW\n"
            "Entry: 1.08500\n"
            "Stop Loss: 1.08000\n"
            "Take Profit 1: 1.09000\n"
            "Take Profit 2: 1.09500";
        Signal sig = parser.parse(text);
        CHECK("T20 Format B valid",         sig.valid);
        CHECK("T20 tp2 1.095",             approx(sig.tp2, 1.095));
    }

    // T21: Format D
    {
        std::string text = "[SIGNAL] USDJPY SELL LIMIT 149.50\nSL: 150.20 | TP: 148.00";
        Signal sig = parser.parse(text);
        CHECK("T21 Format D valid",         sig.valid);
        CHECK("T21 entry 149.50",           approx(sig.entry, 149.50));
    }

    // ── Negative tests ──────────────────────────────────────────────────

    // T22: Garbage
    {
        Signal sig = parser.parse("Good morning everyone! Markets are opening soon.");
        CHECK("T22 Garbage not valid",       !sig.valid);
    }

    // T23: Empty
    {
        Signal sig = parser.parse("");
        CHECK("T23 Empty not valid",         !sig.valid);
    }

    // T24: Just emojis (fire, not cross)
    {
        Signal sig = parser.parse("\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5");
        CHECK("T24 Emoji-only not valid",    !sig.valid);
    }

    // ── Summary ─────────────────────────────────────────────────────────
    std::cout << "\n=== Results: " << g_pass << " passed, "
              << g_fail << " failed out of " << (g_pass + g_fail) << " ===\n";

    return g_fail > 0 ? 1 : 0;
}
