// SignalParser.cpp — Implementation of the scalp signal parser.
// Recognises "Gold sell/buy now" entry signals and break-even commands
// from the RoyalFx Vip channel format.

#include "SignalParser.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_map>

// ─── Signal serialization ────────────────────────────────────────────────────

std::string Signal::serialize() const {
    if (type == SignalType::BREAKEVEN) {
        return "BREAKEVEN|" + symbol;
    }
    if (type == SignalType::CLOSE_ALL) {
        return "CLOSEALL|" + symbol;
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(5);
    os << symbol << "|"
       << (side == Side::BUY ? "BUY" : "SELL") << "|"
       << entry << "|"
       << sl    << "|"
       << tp1   << "|"
       << tp2   << "|"
       << std::setprecision(2) << lots;
    return os.str();
}

// ─── Construction ────────────────────────────────────────────────────────────

SignalParser::SignalParser(double account_balance, double risk_percent,
                           double min_lots, double max_lots)
    : account_balance_(account_balance),
      risk_percent_(risk_percent),
      min_lots_(min_lots),
      max_lots_(max_lots) {}

// ─── Public parse entry point ────────────────────────────────────────────────
// Tries break-even first (short messages), then scalp entry, then legacy formats.

Signal SignalParser::parse(const std::string& raw_text) const {
    Signal sig;

    // 1. Close-all commands (highest priority).
    if (try_close_all(raw_text, sig)) { sig.valid = true; return sig; }

    // 2. Break-even commands (short, distinctive).
    if (try_breakeven(raw_text, sig)) { sig.valid = true; return sig; }

    // 3. Scalp entry: "Gold sell now", "Gold buy now".
    if (try_scalp_entry(raw_text, sig)) { sig.valid = true; }

    // 4. Legacy structured formats (fallback).
    else if (try_format_a(raw_text, sig)) { sig.valid = true; }
    else if (try_format_d(raw_text, sig)) { sig.valid = true; }
    else if (try_format_c(raw_text, sig)) { sig.valid = true; }
    else if (try_format_b(raw_text, sig)) { sig.valid = true; }

    if (sig.valid && sig.type == SignalType::ENTRY && sig.sl != 0.0) {
        double entry_for_lots = sig.entry != 0.0
            ? sig.entry
            : sig.sl;
        sig.lots = calculate_lots(sig.symbol, entry_for_lots, sig.sl);
    } else if (sig.valid && sig.type == SignalType::ENTRY) {
        sig.lots = min_lots_;  // no SL → minimum lot size (overridden by fixed lots)
    }

    return sig;
}

// ─── Scalp entry ─────────────────────────────────────────────────────────────
// Matches:
//   "Gold sell now 🔴"
//   "Gold buy now 🟢"
//   "Gold Buy now 🔷 | xauusd"
//   "Gold sell now 🔴 | xauusd\nScalping ✅\nUse a good MM (money management)"

bool SignalParser::try_scalp_entry(const std::string& text, Signal& sig) const {
    // Case-insensitive match for "gold" followed by "buy" or "sell", then "now".
    static const std::regex re(
        R"(gold\s+(buy|sell)\s+now)",
        std::regex::icase);

    std::smatch m;
    if (!std::regex_search(text, m, re)) return false;

    sig.type   = SignalType::ENTRY;
    sig.symbol = trade_symbol_;  // e.g. "XAUUSD.s"
    sig.entry  = 0.0;           // market order
    sig.sl     = 0.0;           // no SL
    sig.tp1    = 0.0;           // no TP
    sig.tp2    = 0.0;

    std::string side_str = m[1].str();
    std::transform(side_str.begin(), side_str.end(),
                   side_str.begin(), ::toupper);
    sig.side = (side_str == "BUY") ? Signal::Side::BUY : Signal::Side::SELL;

    return true;
}

// ─── Break-even detection ────────────────────────────────────────────────────
// Matches any of:
//   "TP 1 ✅"
//   "+35 pips ✅"   (any number)
//   "Don't forget your BE"
//   "BE hit ✅"
//   "BE HIT 0 pips ✅"
//   "+60 pips from the zone BE or take profits ✅ scalp"

bool SignalParser::try_breakeven(const std::string& text, Signal& sig) const {
    static const std::regex re_tp(
        R"(TP\s*1\s*✅)", std::regex::icase);
    static const std::regex re_pips(
        R"(\+\d+\s*pips?\s*✅)", std::regex::icase);
    static const std::regex re_be_forget(
        R"(forget\s+your\s+BE)", std::regex::icase);
    static const std::regex re_be_hit(
        R"(BE\s+hit)", std::regex::icase);
    static const std::regex re_be_zone(
        R"(from\s+the\s+zone\s+BE)", std::regex::icase);

    bool match = std::regex_search(text, re_tp)
              || std::regex_search(text, re_pips)
              || std::regex_search(text, re_be_forget)
              || std::regex_search(text, re_be_hit)
              || std::regex_search(text, re_be_zone);

    if (!match) return false;

    sig.type   = SignalType::BREAKEVEN;
    sig.symbol = trade_symbol_;
    sig.valid  = true;
    return true;
}

// ─── Close-all detection ─────────────────────────────────────────────────────
// Matches any of:
//   "Close it at the entry point ✅ 0 pips"
//   "❌"  (standalone cross mark)
//   "SL❌"
//   "Close your last current positions"
//   "We cut the trade in positive"

bool SignalParser::try_close_all(const std::string& text, Signal& sig) const {
    // "Close it at the entry point" or "Close your last current positions"
    static const std::regex re_close(
        R"(close\s+(it\s+at|your\s+last))", std::regex::icase);
    // "We cut the trade"
    static const std::regex re_cut(
        R"(we\s+cut\s+the\s+trade)", std::regex::icase);
    // "SL❌" or standalone "❌"
    static const std::regex re_sl_x(
        R"(SL\s*❌)", std::regex::icase);

    // Standalone ❌ — the message is just the cross mark (possibly with whitespace).
    // Must be strict to avoid false positives on messages that happen to contain ❌.
    std::string trimmed = text;
    // Trim whitespace.
    auto start = trimmed.find_first_not_of(" \t\n\r");
    auto end   = trimmed.find_last_not_of(" \t\n\r");
    if (start != std::string::npos)
        trimmed = trimmed.substr(start, end - start + 1);
    bool is_standalone_x = (trimmed == "\xE2\x9D\x8C");  // just "❌"

    bool match = std::regex_search(text, re_close)
              || std::regex_search(text, re_cut)
              || std::regex_search(text, re_sl_x)
              || is_standalone_x;

    if (!match) return false;

    sig.type   = SignalType::CLOSE_ALL;
    sig.symbol = trade_symbol_;
    sig.valid  = true;
    return true;
}

// ─── Format A ────────────────────────────────────────────────────────────────
// XAUUSD BUY @ 2310.50
// SL: 2300.00
// TP: 2330.00
bool SignalParser::try_format_a(const std::string& text, Signal& sig) const {
    static const std::regex re_header(
        R"(([A-Z][A-Z0-9]{2,10})\s+(BUY|SELL)\s*@\s*([0-9]+\.?[0-9]*))",
        std::regex::icase);
    static const std::regex re_sl(
        R"(SL[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);
    static const std::regex re_tp(
        R"(TP[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);

    std::smatch m;
    if (!std::regex_search(text, m, re_header)) return false;

    sig.symbol = m[1].str();
    std::transform(sig.symbol.begin(), sig.symbol.end(),
                   sig.symbol.begin(), ::toupper);

    std::string side_str = m[2].str();
    std::transform(side_str.begin(), side_str.end(),
                   side_str.begin(), ::toupper);
    sig.side = (side_str == "BUY") ? Signal::Side::BUY : Signal::Side::SELL;

    sig.entry = std::stod(m[3].str());

    if (std::regex_search(text, m, re_sl))
        sig.sl = std::stod(m[1].str());
    if (std::regex_search(text, m, re_tp))
        sig.tp1 = std::stod(m[1].str());

    return true;
}

// ─── Format B ────────────────────────────────────────────────────────────────
// 🟢 EURUSD BUY NOW
// Entry: 1.08500
// Stop Loss: 1.08000
// Take Profit 1: 1.09000
// Take Profit 2: 1.09500
bool SignalParser::try_format_b(const std::string& text, Signal& sig) const {
    {
        static const std::regex re_guard(
            R"(Entry|Stop\s*Loss|Take\s*Profit)", std::regex::icase);
        if (!std::regex_search(text, re_guard)) return false;
    }

    static const std::regex re_header(
        R"((?:[\xF0-\xF4][\x80-\xBF]{2}[\x80-\xBF]|[^\w])*\s*([A-Z][A-Z0-9]{2,10})\s+(BUY|SELL)\s*(?:NOW)?)",
        std::regex::icase);
    static const std::regex re_entry(
        R"(Entry[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);
    static const std::regex re_sl(
        R"(Stop\s*Loss[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);
    static const std::regex re_tp1(
        R"(Take\s*Profit\s*1?[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);
    static const std::regex re_tp2(
        R"(Take\s*Profit\s*2[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);

    std::smatch m;
    if (!std::regex_search(text, m, re_header)) return false;

    sig.symbol = m[1].str();
    std::transform(sig.symbol.begin(), sig.symbol.end(),
                   sig.symbol.begin(), ::toupper);

    std::string side_str = m[2].str();
    std::transform(side_str.begin(), side_str.end(),
                   side_str.begin(), ::toupper);
    sig.side = (side_str == "BUY") ? Signal::Side::BUY : Signal::Side::SELL;

    if (std::regex_search(text, m, re_entry))
        sig.entry = std::stod(m[1].str());
    if (std::regex_search(text, m, re_sl))
        sig.sl = std::stod(m[1].str());
    if (std::regex_search(text, m, re_tp1))
        sig.tp1 = std::stod(m[1].str());
    if (std::regex_search(text, m, re_tp2))
        sig.tp2 = std::stod(m[1].str());

    return true;
}

// ─── Format C ────────────────────────────────────────────────────────────────
// SELL GBPUSD
// Price: 1.2650
// SL 1.2700 TP 1.2580
bool SignalParser::try_format_c(const std::string& text, Signal& sig) const {
    {
        static const std::regex re_guard(R"(Price[:\s])", std::regex::icase);
        if (!std::regex_search(text, re_guard)) return false;
    }

    static const std::regex re_header(
        R"((BUY|SELL)\s+([A-Z][A-Z0-9]{2,10}))",
        std::regex::icase);
    static const std::regex re_price(
        R"(Price[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);
    static const std::regex re_sl_tp(
        R"(SL\s+([0-9]+\.?[0-9]*)\s+TP\s+([0-9]+\.?[0-9]*))",
        std::regex::icase);

    std::smatch m;
    if (!std::regex_search(text, m, re_header)) return false;

    std::string side_str = m[1].str();
    std::transform(side_str.begin(), side_str.end(),
                   side_str.begin(), ::toupper);
    sig.side = (side_str == "BUY") ? Signal::Side::BUY : Signal::Side::SELL;

    sig.symbol = m[2].str();
    std::transform(sig.symbol.begin(), sig.symbol.end(),
                   sig.symbol.begin(), ::toupper);

    if (std::regex_search(text, m, re_price))
        sig.entry = std::stod(m[1].str());

    if (std::regex_search(text, m, re_sl_tp)) {
        sig.sl  = std::stod(m[1].str());
        sig.tp1 = std::stod(m[2].str());
    }

    return true;
}

// ─── Format D ────────────────────────────────────────────────────────────────
// [SIGNAL] USDJPY SELL LIMIT 149.50
// SL: 150.20 | TP: 148.00
bool SignalParser::try_format_d(const std::string& text, Signal& sig) const {
    static const std::regex re_header(
        R"(\[SIGNAL\]\s*([A-Z][A-Z0-9]{2,10})\s+(BUY|SELL)\s*(?:LIMIT|STOP)?\s*([0-9]+\.?[0-9]*))",
        std::regex::icase);
    static const std::regex re_sl(
        R"(SL[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);
    static const std::regex re_tp(
        R"(TP[:\s]+([0-9]+\.?[0-9]*))", std::regex::icase);

    std::smatch m;
    if (!std::regex_search(text, m, re_header)) return false;

    sig.symbol = m[1].str();
    std::transform(sig.symbol.begin(), sig.symbol.end(),
                   sig.symbol.begin(), ::toupper);

    std::string side_str = m[2].str();
    std::transform(side_str.begin(), side_str.end(),
                   side_str.begin(), ::toupper);
    sig.side = (side_str == "BUY") ? Signal::Side::BUY : Signal::Side::SELL;

    std::string entry_str = m[3].str();
    if (!entry_str.empty())
        sig.entry = std::stod(entry_str);

    if (std::regex_search(text, m, re_sl))
        sig.sl = std::stod(m[1].str());
    if (std::regex_search(text, m, re_tp))
        sig.tp1 = std::stod(m[1].str());

    return true;
}

// ─── Lot-size calculator ─────────────────────────────────────────────────────

double SignalParser::calculate_lots(const std::string& symbol,
                                     double entry, double sl) const {
    double ps = pip_size(symbol);
    double pv = pip_value(symbol);
    if (ps <= 0.0 || pv <= 0.0) return min_lots_;

    double sl_pips = std::abs(entry - sl) / ps;
    if (sl_pips < 0.1) return min_lots_;

    double risk_amount = account_balance_ * risk_percent_ / 100.0;
    double lots = risk_amount / (sl_pips * pv);

    return std::clamp(lots, min_lots_, max_lots_);
}

// ─── Pip-size lookup ─────────────────────────────────────────────────────────

double SignalParser::pip_size(const std::string& symbol) {
    // Strip broker suffix (e.g. ".s") for lookup.
    std::string base = symbol;
    auto dot = base.find('.');
    if (dot != std::string::npos) base = base.substr(0, dot);

    static const std::unordered_map<std::string, double> table = {
        {"XAUUSD",  0.10},
        {"EURUSD",  0.0001},
        {"GBPUSD",  0.0001},
        {"USDJPY",  0.01},
        {"GBPJPY",  0.01},
        {"NASDAQ",  0.01},
        {"NAS100",  0.01},
        {"US30",    1.0},
        {"USOIL",   0.01},
        {"XAGUSD",  0.001},
        {"AUDUSD",  0.0001},
        {"NZDUSD",  0.0001},
        {"USDCAD",  0.0001},
        {"USDCHF",  0.0001},
        {"EURGBP",  0.0001},
        {"EURJPY",  0.01},
    };
    auto it = table.find(base);
    return (it != table.end()) ? it->second : 0.0001;
}

// ─── Pip-value lookup ────────────────────────────────────────────────────────

double SignalParser::pip_value(const std::string& symbol) {
    std::string base = symbol;
    auto dot = base.find('.');
    if (dot != std::string::npos) base = base.substr(0, dot);

    static const std::unordered_map<std::string, double> table = {
        {"XAUUSD",  10.0},     // $10 per pip (0.10) per lot (100 oz)
        {"EURUSD",  10.0},
        {"GBPUSD",  10.0},
        {"USDJPY",  6.5},
        {"GBPJPY",  6.5},
        {"NASDAQ",  1.0},
        {"NAS100",  1.0},
        {"US30",    1.0},
        {"USOIL",   10.0},
        {"XAGUSD",  50.0},
        {"AUDUSD",  10.0},
        {"NZDUSD",  10.0},
        {"USDCAD",  7.5},
        {"USDCHF",  10.5},
        {"EURGBP",  12.5},
        {"EURJPY",  6.5},
    };
    auto it = table.find(base);
    return (it != table.end()) ? it->second : 10.0;
}
