// TradeJournal.cpp — CSV trade journal for reverse-engineering signal patterns.
// Dumps every trade event with maximum context for offline analysis.
// Author: <your-name>
// Date:   <date>

#include "TradeJournal.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Pip-size lookup (mirrors signal-parser's table).
// ---------------------------------------------------------------------------
double TradeJournal::pip_size(const std::string& symbol) {
    if (symbol.find("JPY") != std::string::npos) return 0.01;
    if (symbol.find("XAU") != std::string::npos) return 0.01;
    if (symbol.find("XAG") != std::string::npos) return 0.001;
    if (symbol == "US30")    return 1.0;
    if (symbol == "NASDAQ" || symbol == "NAS100") return 0.01;
    if (symbol.find("OIL") != std::string::npos) return 0.01;
    return 0.0001;  // default forex
}

// ---------------------------------------------------------------------------
std::string TradeJournal::csv_header() {
    return "event_type,symbol,ticket,side,lots,"
           "open_price,close_price,sl,tp,"
           "profit_usd,pips,pips_to_sl,pips_to_tp,"
           "risk_reward_ratio,"
           "open_time,close_time,duration_secs,duration_human,"
           "bid,ask,spread_pts,"
           "comment";
}

// ---------------------------------------------------------------------------
TradeJournal::TradeJournal(const std::string& csv_path)
    : path_(csv_path) {
    // Check if file already exists (to decide whether to write header).
    struct stat st;
    bool exists = (stat(csv_path.c_str(), &st) == 0 && st.st_size > 0);

    file_.open(csv_path, std::ios::app);
    if (!file_) {
        std::cerr << "[JOURNAL][ERROR] Cannot open CSV: " << csv_path << "\n";
        return;
    }

    if (!exists) {
        file_ << csv_header() << "\n";
        file_.flush();
    }

    std::cerr << "[JOURNAL][INFO] Trade journal: " << csv_path
              << (exists ? " (appending)" : " (new file)") << "\n";
}

TradeJournal::~TradeJournal() {
    if (file_.is_open()) file_.close();
}

// ---------------------------------------------------------------------------
// Append one trade event as a CSV row.
// ---------------------------------------------------------------------------
void TradeJournal::record(const TradeEvent& ev) {
    if (!file_.is_open()) return;

    double ps = pip_size(ev.symbol);
    double pips_moved  = (ev.close_price != 0.0 && ev.open_price != 0.0)
                         ? ev.pips(ps) : 0.0;
    double pips_to_sl  = (ev.sl != 0.0 && ev.open_price != 0.0)
                         ? std::abs(ev.open_price - ev.sl) / ps : 0.0;
    double pips_to_tp  = (ev.tp != 0.0 && ev.open_price != 0.0)
                         ? std::abs(ev.tp - ev.open_price) / ps : 0.0;
    double rr_ratio    = (pips_to_sl > 0.0)
                         ? pips_to_tp / pips_to_sl : 0.0;

    // Format human-readable duration.
    std::string dur_human;
    {
        int64_t s = ev.duration_secs;
        int64_t h = s / 3600;
        int64_t m = (s % 3600) / 60;
        int64_t sec = s % 60;
        std::ostringstream ds;
        if (h > 0) ds << h << "h ";
        if (h > 0 || m > 0) ds << m << "m ";
        ds << sec << "s";
        dur_human = ds.str();
    }

    // Escape comment (replace commas with semicolons for CSV safety).
    std::string safe_comment = ev.comment;
    for (auto& c : safe_comment) {
        if (c == ',') c = ';';
    }

    std::lock_guard<std::mutex> lk(mu_);
    std::ostringstream row;
    row << std::fixed;

    row << ev.type << ","
        << ev.symbol << ","
        << ev.ticket << ","
        << ev.side << ","
        << std::setprecision(2) << ev.lots << ","
        << std::setprecision(5) << ev.open_price << ","
        << std::setprecision(5) << ev.close_price << ","
        << std::setprecision(5) << ev.sl << ","
        << std::setprecision(5) << ev.tp << ","
        << std::setprecision(2) << ev.profit << ","
        << std::setprecision(1) << pips_moved << ","
        << std::setprecision(1) << pips_to_sl << ","
        << std::setprecision(1) << pips_to_tp << ","
        << std::setprecision(2) << rr_ratio << ","
        << ev.open_time << ","
        << ev.close_time << ","
        << ev.duration_secs << ","
        << dur_human << ","
        << std::setprecision(5) << ev.bid << ","
        << std::setprecision(5) << ev.ask << ","
        << std::setprecision(1) << ev.spread << ","
        << safe_comment;

    file_ << row.str() << "\n";
    file_.flush();

    std::cerr << "[JOURNAL][CSV] " << ev.type << " " << ev.side << " "
              << ev.symbol << " P&L=$" << ev.profit
              << " pips=" << pips_moved << "\n";
}
