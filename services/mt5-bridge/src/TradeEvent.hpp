// TradeEvent.hpp — Struct and deserializer for trade events sent by the EA.
// Wire format: EVENT|type|symbol|ticket|side|open_price|close_price|lots|
//              profit|sl|tp|open_time|close_time|duration_secs|
//              bid|ask|spread|comment
// Author: <your-name>
// Date:   <date>
#pragma once

#include <cstdint>
#include <string>

struct TradeEvent {
    std::string type;          // OPENED, CLOSED, SL_HIT, TP_HIT, BREAKEVEN
    std::string symbol;
    uint64_t    ticket    = 0;
    std::string side;          // BUY or SELL
    double open_price     = 0.0;
    double close_price    = 0.0;
    double lots           = 0.0;
    double profit         = 0.0;  // monetary P&L in account currency
    double sl             = 0.0;
    double tp             = 0.0;
    std::string open_time;     // ISO-8601 from EA
    std::string close_time;    // ISO-8601 from EA (empty for OPENED)
    int64_t duration_secs = 0; // seconds the trade was open
    double bid            = 0.0; // bid at event time
    double ask            = 0.0; // ask at event time
    double spread         = 0.0; // spread in points
    std::string comment;
    bool   valid          = false;

    // Compute pips moved: |close - open| / pip_size.
    // pip_size must be provided by the caller.
    double pips(double pip_size) const;
};

// Deserialize the pipe-delimited event string into a TradeEvent.
TradeEvent deserialize_trade_event(const std::string& raw);
