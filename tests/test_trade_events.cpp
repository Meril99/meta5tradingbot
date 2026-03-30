// test_trade_events.cpp — Unit tests for TradeEvent deserialization,
// TelegramNotifier formatting, and TradeJournal CSV output.
// Compile: g++ -std=c++17 test_trade_events.cpp \
//   ../services/mt5-bridge/src/TradeEvent.cpp \
//   ../services/mt5-bridge/src/TradeJournal.cpp \
//   -o test_trade_events
// (TelegramNotifier is tested separately since it needs libcurl.)
// Author: <your-name>
// Date:   <date>

#include "../services/mt5-bridge/src/TradeEvent.hpp"
#include "../services/mt5-bridge/src/TradeJournal.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

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

static bool approx(double a, double b, double eps = 0.01) {
    return std::abs(a - b) < eps;
}

int main() {
    std::cout << "=== TradeEvent & TradeJournal Tests ===\n\n";

    // ── Test 1: Deserialize valid OPENED event ──────────────────────────
    {
        std::string raw =
            "EVENT|OPENED|XAUUSD|12345|BUY|2310.50000|0.00000|0.10|"
            "0.00|2300.00000|2330.00000|2024-01-15T10:30:00||0|"
            "2310.40000|2310.60000|2.0|TelegramBot signal";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T01 valid",            ev.valid);
        CHECK("T01 type OPENED",      ev.type == "OPENED");
        CHECK("T01 symbol XAUUSD",    ev.symbol == "XAUUSD");
        CHECK("T01 ticket 12345",     ev.ticket == 12345);
        CHECK("T01 side BUY",         ev.side == "BUY");
        CHECK("T01 open 2310.50",     approx(ev.open_price, 2310.50));
        CHECK("T01 close 0.0",        approx(ev.close_price, 0.0));
        CHECK("T01 lots 0.10",        approx(ev.lots, 0.10));
        CHECK("T01 sl 2300",          approx(ev.sl, 2300.0));
        CHECK("T01 tp 2330",          approx(ev.tp, 2330.0));
        CHECK("T01 spread 2.0",       approx(ev.spread, 2.0));
    }

    // ── Test 2: Deserialize SL_HIT event ────────────────────────────────
    {
        std::string raw =
            "EVENT|SL_HIT|EURUSD|67890|BUY|1.08500|1.08000|0.20|"
            "-50.00|1.08000|1.09000|2024-01-15T10:30:00|2024-01-15T11:15:00|"
            "2700|1.08005|1.08015|1.0|[sl]";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T02 valid",            ev.valid);
        CHECK("T02 type SL_HIT",      ev.type == "SL_HIT");
        CHECK("T02 profit -50",       approx(ev.profit, -50.0));
        CHECK("T02 duration 2700",    ev.duration_secs == 2700);
        CHECK("T02 pips",             approx(ev.pips(0.0001), 50.0, 1.0));
    }

    // ── Test 3: Deserialize TP_HIT event ────────────────────────────────
    {
        std::string raw =
            "EVENT|TP_HIT|XAUUSD|11111|SELL|2350.00000|2330.00000|0.05|"
            "100.00|2360.00000|2330.00000|2024-01-15T08:00:00|2024-01-15T12:00:00|"
            "14400|2329.90000|2330.10000|3.0|[tp]";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T03 valid",            ev.valid);
        CHECK("T03 type TP_HIT",      ev.type == "TP_HIT");
        CHECK("T03 profit 100",       approx(ev.profit, 100.0));
        CHECK("T03 duration 14400",   ev.duration_secs == 14400);
        CHECK("T03 close_time",       ev.close_time == "2024-01-15T12:00:00");
    }

    // ── Test 4: Deserialize CLOSED event (manual close) ─────────────────
    {
        std::string raw =
            "EVENT|CLOSED|GBPUSD|22222|SELL|1.26500|1.26200|0.30|"
            "90.00|1.27000|1.25800|2024-02-01T09:00:00|2024-02-01T09:45:00|"
            "2700|1.26195|1.26210|1.5|manual close";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T04 valid",            ev.valid);
        CHECK("T04 type CLOSED",      ev.type == "CLOSED");
        CHECK("T04 symbol GBPUSD",    ev.symbol == "GBPUSD");
        CHECK("T04 side SELL",        ev.side == "SELL");
        CHECK("T04 profit 90",        approx(ev.profit, 90.0));
    }

    // ── Test 5: Deserialize BREAKEVEN event ─────────────────────────────
    {
        std::string raw =
            "EVENT|BREAKEVEN|USDJPY|33333|BUY|149.50000|0.00000|0.15|"
            "0.00|149.50000|150.50000|2024-02-01T10:00:00||"
            "3600|149.55000|149.56000|2.0|SL moved to entry";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T05 valid",            ev.valid);
        CHECK("T05 type BREAKEVEN",   ev.type == "BREAKEVEN");
        CHECK("T05 sl == open",       approx(ev.sl, ev.open_price));
    }

    // ── Test 6: Malformed event (too few fields) ────────────────────────
    {
        std::string raw = "EVENT|OPENED|XAUUSD|12345";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T06 malformed invalid", !ev.valid);
    }

    // ── Test 7: Missing EVENT prefix ────────────────────────────────────
    {
        std::string raw =
            "XAUUSD|BUY|2310.50|2300.00|2330.00|0.00|0.10";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T07 no EVENT prefix",  !ev.valid);
    }

    // ── Test 8: Empty string ────────────────────────────────────────────
    {
        TradeEvent ev = deserialize_trade_event("");
        CHECK("T08 empty invalid",    !ev.valid);
    }

    // ── Test 9: Pips calculation — XAUUSD (pip = 0.01) ──────────────────
    {
        TradeEvent ev;
        ev.open_price  = 2310.50;
        ev.close_price = 2330.00;
        double p = ev.pips(0.01);
        CHECK("T09 XAUUSD pips",      approx(p, 1950.0, 1.0));
    }

    // ── Test 10: Pips calculation — EURUSD (pip = 0.0001) ───────────────
    {
        TradeEvent ev;
        ev.open_price  = 1.08500;
        ev.close_price = 1.08000;
        double p = ev.pips(0.0001);
        CHECK("T10 EURUSD pips",      approx(p, 50.0, 1.0));
    }

    // ── Test 11: TradeJournal writes CSV header ─────────────────────────
    {
        const char* test_csv = "/tmp/test_journal.csv";
        std::remove(test_csv);
        {
            TradeJournal journal(test_csv);

            TradeEvent ev;
            ev.type = "OPENED"; ev.symbol = "XAUUSD"; ev.ticket = 99999;
            ev.side = "BUY"; ev.open_price = 2310.50; ev.close_price = 0.0;
            ev.lots = 0.10; ev.profit = 0.0; ev.sl = 2300.0; ev.tp = 2330.0;
            ev.open_time = "2024-01-15T10:30:00"; ev.close_time = "";
            ev.duration_secs = 0; ev.bid = 2310.40; ev.ask = 2310.60;
            ev.spread = 2.0; ev.comment = "test"; ev.valid = true;

            journal.record(ev);
        }

        std::ifstream f(test_csv);
        std::string line1, line2;
        std::getline(f, line1);  // header
        std::getline(f, line2);  // data row
        CHECK("T11 CSV header exists",  line1.find("event_type") != std::string::npos);
        CHECK("T11 CSV has columns",    line1.find("risk_reward_ratio") != std::string::npos);
        CHECK("T11 CSV data row",       line2.find("OPENED") != std::string::npos);
        CHECK("T11 CSV has XAUUSD",     line2.find("XAUUSD") != std::string::npos);
        CHECK("T11 CSV has ticket",     line2.find("99999") != std::string::npos);
        std::remove(test_csv);
    }

    // ── Test 12: TradeJournal appends (no duplicate header) ─────────────
    {
        const char* test_csv = "/tmp/test_journal_append.csv";
        std::remove(test_csv);

        TradeEvent ev;
        ev.type = "CLOSED"; ev.symbol = "EURUSD"; ev.ticket = 11111;
        ev.side = "SELL"; ev.open_price = 1.08500; ev.close_price = 1.08000;
        ev.lots = 0.20; ev.profit = 100.0; ev.sl = 1.09000; ev.tp = 1.07500;
        ev.open_time = "2024-01-15T10:00:00";
        ev.close_time = "2024-01-15T12:00:00";
        ev.duration_secs = 7200; ev.bid = 1.08; ev.ask = 1.08010;
        ev.spread = 1.0; ev.comment = ""; ev.valid = true;

        // Write two records in separate journal instances.
        {
            TradeJournal j1(test_csv);
            j1.record(ev);
        }
        {
            TradeJournal j2(test_csv);
            ev.ticket = 22222;
            j2.record(ev);
        }

        // Count lines.
        std::ifstream f(test_csv);
        int lines = 0;
        std::string line;
        while (std::getline(f, line)) lines++;
        CHECK("T12 3 lines (header+2)", lines == 3);
        std::remove(test_csv);
    }

    // ── Test 13: Risk/reward ratio in CSV ───────────────────────────────
    {
        const char* test_csv = "/tmp/test_journal_rr.csv";
        std::remove(test_csv);
        {
            TradeJournal journal(test_csv);
            TradeEvent ev;
            ev.type = "TP_HIT"; ev.symbol = "EURUSD"; ev.ticket = 55555;
            ev.side = "BUY"; ev.open_price = 1.08000; ev.close_price = 1.09000;
            ev.lots = 0.10; ev.profit = 100.0;
            ev.sl = 1.07500;   // 50 pips SL
            ev.tp = 1.09000;   // 100 pips TP → R:R = 2.0
            ev.open_time = "2024-01-15T10:00:00";
            ev.close_time = "2024-01-15T14:00:00";
            ev.duration_secs = 14400; ev.bid = 1.09; ev.ask = 1.09010;
            ev.spread = 1.0; ev.comment = "[tp]"; ev.valid = true;
            journal.record(ev);
        }
        std::ifstream f(test_csv);
        std::string header, data;
        std::getline(f, header);
        std::getline(f, data);
        // R:R should be 2.00 (100 pips TP / 50 pips SL)
        CHECK("T13 R:R in CSV",  data.find("2.00") != std::string::npos);
        std::remove(test_csv);
    }

    // ── Test 14: Large ticket number ────────────────────────────────────
    {
        std::string raw =
            "EVENT|OPENED|XAUUSD|99999999999|BUY|2310.50000|0.00000|0.10|"
            "0.00|2300.00000|2330.00000|2024-01-15T10:30:00||0|"
            "2310.40000|2310.60000|2.0|test";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T14 large ticket",     ev.valid && ev.ticket == 99999999999ULL);
    }

    // ── Test 15: Zero profit event ──────────────────────────────────────
    {
        std::string raw =
            "EVENT|CLOSED|EURUSD|44444|BUY|1.08500|1.08500|0.10|"
            "0.00|1.08000|1.09000|2024-01-15T10:00:00|2024-01-15T10:05:00|"
            "300|1.08500|1.08510|1.0|breakeven close";
        TradeEvent ev = deserialize_trade_event(raw);
        CHECK("T15 zero profit",      ev.valid && approx(ev.profit, 0.0));
        CHECK("T15 zero pips",        approx(ev.pips(0.0001), 0.0));
    }

    // ── Summary ─────────────────────────────────────────────────────────
    std::cout << "\n=== Results: " << g_pass << " passed, "
              << g_fail << " failed out of " << (g_pass + g_fail) << " ===\n";

    return g_fail > 0 ? 1 : 0;
}
