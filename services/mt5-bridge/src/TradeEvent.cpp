// TradeEvent.cpp — Trade event deserialization.
// Author: <your-name>
// Date:   <date>

#include "TradeEvent.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

double TradeEvent::pips(double pip_size) const {
    if (pip_size <= 0.0) return 0.0;
    return std::abs(close_price - open_price) / pip_size;
}

// Parse: EVENT|type|symbol|ticket|side|open|close|lots|profit|sl|tp|
//        open_time|close_time|duration|bid|ask|spread|comment
TradeEvent deserialize_trade_event(const std::string& raw) {
    TradeEvent ev;
    std::vector<std::string> parts;
    std::istringstream ss(raw);
    std::string token;

    while (std::getline(ss, token, '|')) {
        parts.push_back(token);
    }

    // Expect at least 18 fields (EVENT tag + 17 data fields).
    if (parts.size() < 18 || parts[0] != "EVENT") {
        std::cerr << "[MT5-BRIDGE][ERROR] Malformed trade event: " << raw << "\n";
        return ev;
    }

    ev.type          = parts[1];
    ev.symbol        = parts[2];
    ev.ticket        = std::stoull(parts[3]);
    ev.side          = parts[4];
    ev.open_price    = std::stod(parts[5]);
    ev.close_price   = std::stod(parts[6]);
    ev.lots          = std::stod(parts[7]);
    ev.profit        = std::stod(parts[8]);
    ev.sl            = std::stod(parts[9]);
    ev.tp            = std::stod(parts[10]);
    ev.open_time     = parts[11];
    ev.close_time    = parts[12];
    ev.duration_secs = std::stoll(parts[13]);
    ev.bid           = std::stod(parts[14]);
    ev.ask           = std::stod(parts[15]);
    ev.spread        = std::stod(parts[16]);
    ev.comment       = parts[17];
    ev.valid         = true;

    return ev;
}
