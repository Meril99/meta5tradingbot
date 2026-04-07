// SignalParser.hpp — Regex-based trading signal parser.
// Handles scalp signals from Telegram channels (e.g. "Gold sell now")
// and break-even commands (e.g. "TP 1 ✅", "Don't forget your BE").
#pragma once

#include <cmath>
#include <string>

// Type of parsed message.
enum class SignalType { ENTRY, BREAKEVEN, CLOSE_ALL };

// Represents a parsed trading signal.
struct Signal {
    SignalType  type = SignalType::ENTRY;
    std::string symbol;                        // e.g. "XAUUSD.s"
    enum class Side { BUY, SELL } side = Side::BUY;
    double entry = 0.0;                        // 0.0 = market order
    double sl    = 0.0;                        // stop loss
    double tp1   = 0.0;                        // take profit 1
    double tp2   = 0.0;                        // take profit 2 (optional)
    double lots  = 0.0;                        // filled by caller
    bool   valid = false;                      // true if parsing succeeded

    // Serialize to pipe-delimited string for ZMQ transport.
    // ENTRY:     SYMBOL|SIDE|entry|sl|tp1|tp2|lots
    // BREAKEVEN: BREAKEVEN|SYMBOL
    // CLOSE_ALL: CLOSEALL|SYMBOL
    std::string serialize() const;
};

class SignalParser {
public:
    // Construct with risk-management parameters (from .env).
    SignalParser(double account_balance, double risk_percent,
                 double min_lots, double max_lots);

    // Set the trade symbol to use (e.g. "XAUUSD.s").
    void set_trade_symbol(const std::string& sym) { trade_symbol_ = sym; }

    // Parse a raw Telegram message into a Signal struct.
    // Returns a Signal with valid==true on success.
    Signal parse(const std::string& raw_text) const;

private:
    // Scalp entry: "Gold sell now", "Gold buy now", etc.
    bool try_scalp_entry(const std::string& text, Signal& sig) const;

    // Break-even commands: "TP 1 ✅", "+35 pips ✅", "Don't forget your BE", etc.
    bool try_breakeven(const std::string& text, Signal& sig) const;

    // Close-all commands: "❌", "SL❌", "Close your last current positions", etc.
    bool try_close_all(const std::string& text, Signal& sig) const;

    // Legacy structured formats (kept as fallback).
    bool try_format_a(const std::string& text, Signal& sig) const;
    bool try_format_b(const std::string& text, Signal& sig) const;
    bool try_format_c(const std::string& text, Signal& sig) const;
    bool try_format_d(const std::string& text, Signal& sig) const;

    // Calculate lot size from risk parameters and SL distance.
    double calculate_lots(const std::string& symbol,
                          double entry, double sl) const;

    // Lookup pip size for a symbol (e.g. XAUUSD → 0.10).
    static double pip_size(const std::string& symbol);

    // Lookup pip value in USD for a symbol at 1 standard lot.
    static double pip_value(const std::string& symbol);

    std::string trade_symbol_ = "XAUUSD.s";  // default, overridden by env
    double account_balance_;
    double risk_percent_;
    double min_lots_;
    double max_lots_;
};
