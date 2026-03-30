// SignalParser.hpp — Regex-based trading signal parser.
// Handles multiple common signal text formats from Telegram channels.
// Author: <your-name>
// Date:   <date>
#pragma once

#include <cmath>
#include <string>

// Represents a parsed trading signal.
struct Signal {
    std::string symbol;                        // e.g. "XAUUSD"
    enum class Side { BUY, SELL } side = Side::BUY;
    double entry = 0.0;                        // 0.0 = market order
    double sl    = 0.0;                        // stop loss
    double tp1   = 0.0;                        // take profit 1
    double tp2   = 0.0;                        // take profit 2 (optional)
    double lots  = 0.0;                        // filled by risk manager
    bool   valid = false;                      // true if parsing succeeded

    // Serialize to pipe-delimited string for ZMQ transport.
    std::string serialize() const;
};

class SignalParser {
public:
    // Construct with risk-management parameters (from .env).
    SignalParser(double account_balance, double risk_percent,
                 double min_lots, double max_lots);

    // Parse a raw Telegram message into a Signal struct.
    // Returns a Signal with valid==true on success.
    Signal parse(const std::string& raw_text) const;

private:
    // Try each known format in order; return true on first match.
    bool try_format_a(const std::string& text, Signal& sig) const;
    bool try_format_b(const std::string& text, Signal& sig) const;
    bool try_format_c(const std::string& text, Signal& sig) const;
    bool try_format_d(const std::string& text, Signal& sig) const;

    // Calculate lot size from risk parameters and SL distance.
    double calculate_lots(const std::string& symbol,
                          double entry, double sl) const;

    // Lookup pip size for a symbol (e.g. XAUUSD → 0.01).
    static double pip_size(const std::string& symbol);

    // Lookup pip value in USD for a symbol at 1 standard lot.
    static double pip_value(const std::string& symbol);

    double account_balance_;
    double risk_percent_;
    double min_lots_;
    double max_lots_;
};
