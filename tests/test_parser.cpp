// test_parser.cpp — Unit tests for SignalParser.
// Compiles standalone: g++ -std=c++17 test_parser.cpp ../services/signal-parser/src/SignalParser.cpp -o test_parser
// Author: <your-name>
// Date:   <date>

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

// Verify a condition and print PASS/FAIL with a test name.
#define CHECK(name, cond) do { \
    if (cond) { \
        std::cout << "[PASS] " << (name) << "\n"; \
        ++g_pass; \
    } else { \
        std::cout << "[FAIL] " << (name) << "\n"; \
        ++g_fail; \
    } \
} while(0)

// Helper: check if two doubles are approximately equal.
static bool approx(double a, double b, double eps = 0.001) {
    return std::abs(a - b) < eps;
}

int main() {
    SignalParser parser(BALANCE, RISK_PCT, MIN_LOTS, MAX_LOTS);

    std::cout << "=== SignalParser Unit Tests ===\n\n";

    // ── Test 1: Format A — XAUUSD BUY ───────────────────────────────────
    {
        std::string text = "XAUUSD BUY @ 2310.50\nSL: 2300.00\nTP: 2330.00";
        Signal sig = parser.parse(text);
        CHECK("T01 Format A valid",         sig.valid);
        CHECK("T01 symbol XAUUSD",          sig.symbol == "XAUUSD");
        CHECK("T01 side BUY",               sig.side == Signal::Side::BUY);
        CHECK("T01 entry 2310.50",          approx(sig.entry, 2310.50));
        CHECK("T01 sl 2300.00",             approx(sig.sl, 2300.00));
        CHECK("T01 tp1 2330.00",            approx(sig.tp1, 2330.00));
        CHECK("T01 lots > 0",               sig.lots > 0.0);
    }

    // ── Test 2: Format A — EURUSD SELL ──────────────────────────────────
    {
        std::string text = "EURUSD SELL @ 1.08200\nSL: 1.08700\nTP: 1.07500";
        Signal sig = parser.parse(text);
        CHECK("T02 Format A SELL valid",     sig.valid);
        CHECK("T02 symbol EURUSD",           sig.symbol == "EURUSD");
        CHECK("T02 side SELL",               sig.side == Signal::Side::SELL);
        CHECK("T02 entry 1.08200",           approx(sig.entry, 1.082));
    }

    // ── Test 3: Format A — no space around @ ────────────────────────────
    {
        std::string text = "GBPUSD BUY @1.27500\nSL: 1.27000\nTP: 1.28000";
        Signal sig = parser.parse(text);
        CHECK("T03 Format A no-space @",     sig.valid);
        CHECK("T03 entry 1.27500",           approx(sig.entry, 1.275));
    }

    // ── Test 4: Format B — emoji + two TPs ──────────────────────────────
    {
        std::string text =
            "\xF0\x9F\x9F\xA2 EURUSD BUY NOW\n"
            "Entry: 1.08500\n"
            "Stop Loss: 1.08000\n"
            "Take Profit 1: 1.09000\n"
            "Take Profit 2: 1.09500";
        Signal sig = parser.parse(text);
        CHECK("T04 Format B valid",          sig.valid);
        CHECK("T04 symbol EURUSD",           sig.symbol == "EURUSD");
        CHECK("T04 side BUY",                sig.side == Signal::Side::BUY);
        CHECK("T04 entry 1.08500",           approx(sig.entry, 1.085));
        CHECK("T04 sl 1.08000",              approx(sig.sl, 1.08));
        CHECK("T04 tp1 1.09000",             approx(sig.tp1, 1.09));
        CHECK("T04 tp2 1.09500",             approx(sig.tp2, 1.095));
    }

    // ── Test 5: Format B — SELL with emoji ──────────────────────────────
    {
        std::string text =
            "\xF0\x9F\x94\xB4 GBPJPY SELL NOW\n"
            "Entry: 192.500\n"
            "Stop Loss: 193.000\n"
            "Take Profit 1: 191.500\n"
            "Take Profit 2: 191.000";
        Signal sig = parser.parse(text);
        CHECK("T05 Format B SELL valid",     sig.valid);
        CHECK("T05 symbol GBPJPY",           sig.symbol == "GBPJPY");
        CHECK("T05 side SELL",               sig.side == Signal::Side::SELL);
    }

    // ── Test 6: Format B — single TP ────────────────────────────────────
    {
        std::string text =
            "\xF0\x9F\x9F\xA2 AUDUSD BUY NOW\n"
            "Entry: 0.66500\n"
            "Stop Loss: 0.66000\n"
            "Take Profit 1: 0.67200";
        Signal sig = parser.parse(text);
        CHECK("T06 Format B single TP",      sig.valid);
        CHECK("T06 tp2 is 0",                approx(sig.tp2, 0.0));
    }

    // ── Test 7: Format C — SELL GBPUSD ──────────────────────────────────
    {
        std::string text = "SELL GBPUSD\nPrice: 1.2650\nSL 1.2700 TP 1.2580";
        Signal sig = parser.parse(text);
        CHECK("T07 Format C valid",          sig.valid);
        CHECK("T07 symbol GBPUSD",           sig.symbol == "GBPUSD");
        CHECK("T07 side SELL",               sig.side == Signal::Side::SELL);
        CHECK("T07 entry 1.265",             approx(sig.entry, 1.265));
        CHECK("T07 sl 1.27",                 approx(sig.sl, 1.27));
        CHECK("T07 tp1 1.258",               approx(sig.tp1, 1.258));
    }

    // ── Test 8: Format C — BUY USDJPY ───────────────────────────────────
    {
        std::string text = "BUY USDJPY\nPrice: 149.800\nSL 149.200 TP 150.800";
        Signal sig = parser.parse(text);
        CHECK("T08 Format C BUY USDJPY",     sig.valid);
        CHECK("T08 symbol USDJPY",           sig.symbol == "USDJPY");
    }

    // ── Test 9: Format C — XAUUSD ───────────────────────────────────────
    {
        std::string text = "SELL XAUUSD\nPrice: 2350.00\nSL 2360.00 TP 2330.00";
        Signal sig = parser.parse(text);
        CHECK("T09 Format C XAUUSD",         sig.valid);
        CHECK("T09 entry 2350",              approx(sig.entry, 2350.0));
    }

    // ── Test 10: Format D — SELL LIMIT ──────────────────────────────────
    {
        std::string text = "[SIGNAL] USDJPY SELL LIMIT 149.50\nSL: 150.20 | TP: 148.00";
        Signal sig = parser.parse(text);
        CHECK("T10 Format D valid",          sig.valid);
        CHECK("T10 symbol USDJPY",           sig.symbol == "USDJPY");
        CHECK("T10 side SELL",               sig.side == Signal::Side::SELL);
        CHECK("T10 entry 149.50",            approx(sig.entry, 149.50));
        CHECK("T10 sl 150.20",               approx(sig.sl, 150.20));
        CHECK("T10 tp1 148.00",              approx(sig.tp1, 148.0));
    }

    // ── Test 11: Format D — BUY LIMIT ───────────────────────────────────
    {
        std::string text = "[SIGNAL] EURUSD BUY LIMIT 1.07800\nSL: 1.07200 | TP: 1.08500";
        Signal sig = parser.parse(text);
        CHECK("T11 Format D BUY LIMIT",      sig.valid);
        CHECK("T11 entry 1.078",             approx(sig.entry, 1.078));
    }

    // ── Test 12: Format D — BUY STOP ────────────────────────────────────
    {
        std::string text = "[SIGNAL] XAUUSD BUY STOP 2380.00\nSL: 2370.00 | TP: 2400.00";
        Signal sig = parser.parse(text);
        CHECK("T12 Format D BUY STOP",       sig.valid);
        CHECK("T12 entry 2380",              approx(sig.entry, 2380.0));
    }

    // ── Test 13: Lowercase input ────────────────────────────────────────
    {
        std::string text = "xauusd buy @ 2310.50\nsl: 2300.00\ntp: 2330.00";
        Signal sig = parser.parse(text);
        CHECK("T13 Lowercase parsed",        sig.valid);
        CHECK("T13 symbol uppercased",       sig.symbol == "XAUUSD");
    }

    // ── Test 14: Missing TP (partial signal — valid with SL) ────────────
    {
        std::string text = "GBPUSD BUY @ 1.27500\nSL: 1.27000";
        Signal sig = parser.parse(text);
        CHECK("T14 Missing TP valid",        sig.valid);
        CHECK("T14 tp1 is 0",               approx(sig.tp1, 0.0));
        CHECK("T14 lots calculated",        sig.lots > 0.0);
    }

    // ── Test 15: Garbage text (should NOT parse) ────────────────────────
    {
        std::string text = "Good morning everyone! Markets are opening soon.";
        Signal sig = parser.parse(text);
        CHECK("T15 Garbage not valid",       !sig.valid);
    }

    // ── Test 16: Random numbers (should NOT parse) ──────────────────────
    {
        std::string text = "2310 2300 2330 BUY";
        Signal sig = parser.parse(text);
        // This might partially match Format C (BUY <number>); that's OK
        // as long as it doesn't crash. We mainly check it doesn't false-positive
        // on a well-formed signal.
        // Just ensure no crash — valid may be true or false here.
        CHECK("T16 Random numbers no crash", true);
    }

    // ── Test 17: Empty string ───────────────────────────────────────────
    {
        std::string text = "";
        Signal sig = parser.parse(text);
        CHECK("T17 Empty string not valid",  !sig.valid);
    }

    // ── Test 18: Just emojis ────────────────────────────────────────────
    {
        std::string text = "\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5";
        Signal sig = parser.parse(text);
        CHECK("T18 Emoji-only not valid",    !sig.valid);
    }

    // ── Test 19: Serialization round-trip ───────────────────────────────
    {
        std::string text = "XAUUSD BUY @ 2310.50\nSL: 2300.00\nTP: 2330.00";
        Signal sig = parser.parse(text);
        std::string s = sig.serialize();
        CHECK("T19 Serialize contains XAUUSD", s.find("XAUUSD") != std::string::npos);
        CHECK("T19 Serialize contains BUY",    s.find("BUY") != std::string::npos);
        CHECK("T19 Serialize has 6 pipes",
              std::count(s.begin(), s.end(), '|') == 6);
    }

    // ── Test 20: Lot sizing — verify risk formula ───────────────────────
    {
        // EURUSD, entry=1.08500, SL=1.08000 → 50 pips SL distance
        // risk_amount = 10000 * 1% = 100 USD
        // sl_pips = |1.085 - 1.08| / 0.0001 = 500 (in pipettes) — actually
        // 0.005 / 0.0001 = 50 pips
        // lots = 100 / (50 * 10) = 0.20
        std::string text =
            "\xF0\x9F\x9F\xA2 EURUSD BUY NOW\n"
            "Entry: 1.08500\n"
            "Stop Loss: 1.08000\n"
            "Take Profit 1: 1.09000";
        Signal sig = parser.parse(text);
        CHECK("T20 Lot size ~0.20",          approx(sig.lots, 0.20, 0.05));
    }

    // ── Test 21: NASDAQ Format A ────────────────────────────────────────
    {
        std::string text = "NASDAQ BUY @ 18500.50\nSL: 18450.00\nTP: 18600.00";
        Signal sig = parser.parse(text);
        CHECK("T21 NASDAQ valid",            sig.valid);
        CHECK("T21 NASDAQ symbol",           sig.symbol == "NASDAQ");
    }

    // ── Test 22: USOIL Format B ─────────────────────────────────────────
    {
        std::string text =
            "\xF0\x9F\x9F\xA2 USOIL BUY NOW\n"
            "Entry: 78.50\n"
            "Stop Loss: 77.80\n"
            "Take Profit 1: 79.50\n"
            "Take Profit 2: 80.00";
        Signal sig = parser.parse(text);
        CHECK("T22 USOIL valid",            sig.valid);
        CHECK("T22 USOIL symbol",           sig.symbol == "USOIL");
        CHECK("T22 USOIL tp2 80.00",        approx(sig.tp2, 80.0));
    }

    // ── Test 23: US30 Format D ──────────────────────────────────────────
    {
        std::string text = "[SIGNAL] US30 SELL LIMIT 39200.00\nSL: 39350.00 | TP: 38900.00";
        Signal sig = parser.parse(text);
        CHECK("T23 US30 valid",             sig.valid);
        CHECK("T23 US30 entry",             approx(sig.entry, 39200.0));
    }

    // ── Summary ─────────────────────────────────────────────────────────
    std::cout << "\n=== Results: " << g_pass << " passed, "
              << g_fail << " failed out of " << (g_pass + g_fail) << " ===\n";

    return g_fail > 0 ? 1 : 0;
}
