// TradeJournal.hpp — Appends every trade event to a CSV file for analysis.
// CSV contains full context: times, OHLC at entry/exit, pips, P&L, spread, etc.
// Author: <your-name>
// Date:   <date>
#pragma once

#include "TradeEvent.hpp"

#include <fstream>
#include <mutex>
#include <string>

class TradeJournal {
public:
    // csv_path: file to append to (created with header if it doesn't exist).
    explicit TradeJournal(const std::string& csv_path);
    ~TradeJournal();

    TradeJournal(const TradeJournal&) = delete;
    TradeJournal& operator=(const TradeJournal&) = delete;

    // Append a trade event row. Thread-safe.
    void record(const TradeEvent& ev);

    // Return the CSV header string (for reference / new files).
    static std::string csv_header();

private:
    std::ofstream file_;
    std::mutex    mu_;
    std::string   path_;

    // Compute pip size for a symbol (simplified lookup).
    static double pip_size(const std::string& symbol);
};
