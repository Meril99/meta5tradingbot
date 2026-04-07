// main.cpp — Entry point for signal-parser service.
// Receives raw Telegram messages via ZMQ PULL, parses them into structured
// trading signals, and forwards them to mt5-bridge via ZMQ PUSH.

#include "SignalParser.hpp"
#include "ZmqBridge.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

std::string require_env(const char* name) {
    const char* val = std::getenv(name);
    if (!val || val[0] == '\0') {
        std::cerr << "[PARSER][FATAL] Missing env var: " << name << "\n";
        std::exit(1);
    }
    return val;
}

double require_env_double(const char* name) {
    return std::stod(require_env(name));
}

std::string optional_env(const char* name) {
    const char* val = std::getenv(name);
    return (val && val[0] != '\0') ? val : "";
}

std::vector<double> parse_fixed_lots(const std::string& csv) {
    std::vector<double> lots;
    std::istringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (!token.empty())
            lots.push_back(std::stod(token));
    }
    return lots;
}
}  // namespace

int main() {
    std::cerr << "[PARSER][INFO] Starting signal-parser\n";

    std::string pull_port = require_env("ZMQ_LISTENER_PORT");
    std::string push_port = require_env("ZMQ_PARSER_PORT");

    // Fixed lots mode.
    std::string fixed_lots_csv = optional_env("FIXED_LOTS");
    std::vector<double> fixed_lots = parse_fixed_lots(fixed_lots_csv);

    // Trade symbol (e.g. "XAUUSD.s" for PU Prime).
    std::string trade_symbol = optional_env("TRADE_SYMBOL");
    if (trade_symbol.empty()) trade_symbol = "XAUUSD.s";

    double balance = 0, risk_pct = 0, min_lots = 0, max_lots = 0;
    if (fixed_lots.empty()) {
        balance  = require_env_double("ACCOUNT_BALANCE");
        risk_pct = require_env_double("RISK_PERCENT");
        min_lots = require_env_double("MIN_LOTS");
        max_lots = require_env_double("MAX_LOTS");
    } else {
        std::cerr << "[PARSER][INFO] Fixed lots mode: ";
        for (size_t i = 0; i < fixed_lots.size(); ++i)
            std::cerr << (i ? ", " : "") << fixed_lots[i];
        std::cerr << "\n";
    }

    std::string pull_addr = "tcp://127.0.0.1:" + pull_port;
    std::string push_addr = "tcp://0.0.0.0:" + push_port;

    ZmqBridge bridge(pull_addr, push_addr);
    SignalParser parser(balance, risk_pct, min_lots, max_lots);
    parser.set_trade_symbol(trade_symbol);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cerr << "[PARSER][INFO] Symbol: " << trade_symbol << "\n";
    std::cerr << "[PARSER][INFO] Listening on " << pull_addr
              << ", forwarding to " << push_addr << "\n";

    while (g_running) {
        std::string raw = bridge.receive();
        if (raw.empty()) continue;

        std::cerr << "[PARSER][RAW] " << raw.substr(0, 120) << "\n";

        Signal sig = parser.parse(raw);
        if (!sig.valid) {
            std::cerr << "[PARSER][SKIP] Could not parse signal from message\n";
            continue;
        }

        // Break-even or close-all: send once (no lots needed).
        if (sig.type == SignalType::BREAKEVEN) {
            std::string serialized = sig.serialize();
            std::cerr << "[PARSER][BREAKEVEN] " << serialized << "\n";
            bridge.send(serialized);
            continue;
        }
        if (sig.type == SignalType::CLOSE_ALL) {
            std::string serialized = sig.serialize();
            std::cerr << "[PARSER][CLOSEALL] " << serialized << "\n";
            bridge.send(serialized);
            continue;
        }

        // Entry: send one signal per fixed lot size.
        if (!fixed_lots.empty()) {
            for (double lot : fixed_lots) {
                sig.lots = lot;
                std::string serialized = sig.serialize();
                std::cerr << "[PARSER][SIGNAL] " << serialized << "\n";
                bridge.send(serialized);
            }
        } else {
            std::string serialized = sig.serialize();
            std::cerr << "[PARSER][SIGNAL] " << serialized << "\n";
            bridge.send(serialized);
        }
    }

    std::cerr << "[PARSER][INFO] Shutting down.\n";
    return 0;
}
