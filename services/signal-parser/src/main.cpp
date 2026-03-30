// main.cpp — Entry point for signal-parser service.
// Receives raw Telegram messages via ZMQ PULL, parses them into structured
// trading signals, and forwards them to mt5-bridge via ZMQ PUSH.
// Author: <your-name>
// Date:   <date>

#include "SignalParser.hpp"
#include "ZmqBridge.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

// Read an environment variable or abort.
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
}  // namespace

int main() {
    std::cerr << "[PARSER][INFO] Starting signal-parser\n";

    std::string pull_port = require_env("ZMQ_LISTENER_PORT");
    std::string push_port = require_env("ZMQ_PARSER_PORT");
    double balance    = require_env_double("ACCOUNT_BALANCE");
    double risk_pct   = require_env_double("RISK_PERCENT");
    double min_lots   = require_env_double("MIN_LOTS");
    double max_lots   = require_env_double("MAX_LOTS");

    std::string pull_addr = "tcp://127.0.0.1:" + pull_port;
    std::string push_addr = "tcp://0.0.0.0:" + push_port;

    ZmqBridge bridge(pull_addr, push_addr);
    SignalParser parser(balance, risk_pct, min_lots, max_lots);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cerr << "[PARSER][INFO] Listening on " << pull_addr
              << ", forwarding to " << push_addr << "\n";

    // Main loop: pull raw text, parse, push serialized signal.
    while (g_running) {
        std::string raw = bridge.receive();
        if (raw.empty()) continue;  // timeout, loop to check g_running

        std::cerr << "[PARSER][RAW] " << raw.substr(0, 120) << "\n";

        Signal sig = parser.parse(raw);
        if (!sig.valid) {
            std::cerr << "[PARSER][SKIP] Could not parse signal from message\n";
            continue;
        }

        std::string serialized = sig.serialize();
        std::cerr << "[PARSER][SIGNAL] " << serialized << "\n";
        bridge.send(serialized);
    }

    std::cerr << "[PARSER][INFO] Shutting down.\n";
    return 0;
}
