// main.cpp — Entry point for tdlib-listener service.
// Initializes TDLib, connects to Telegram, and forwards channel messages
// to the signal-parser via ZeroMQ PUSH.
// Author: <your-name>
// Date:   <date>

#include "TelegramClient.hpp"
#include "ZmqPublisher.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
std::unique_ptr<TelegramClient> g_client;

void signal_handler(int) {
    if (g_client) g_client->stop();
}

// Read an environment variable or abort with an error message.
std::string require_env(const char* name) {
    const char* val = std::getenv(name);
    if (!val || val[0] == '\0') {
        std::cerr << "[LISTENER][FATAL] Missing environment variable: "
                  << name << "\n";
        std::exit(1);
    }
    return val;
}
}  // namespace

int main() {
    std::cerr << "[LISTENER][INFO] Starting tdlib-listener\n";

    // Load configuration from environment.
    std::string api_id     = require_env("TELEGRAM_API_ID");
    std::string api_hash   = require_env("TELEGRAM_API_HASH");
    std::string phone      = require_env("TELEGRAM_PHONE");
    std::string channel_s  = require_env("TELEGRAM_CHANNEL_ID");
    std::string zmq_port   = require_env("ZMQ_LISTENER_PORT");

    int64_t channel_id = std::stoll(channel_s);
    std::string bind_addr = "tcp://0.0.0.0:" + zmq_port;

    // Create the ZeroMQ publisher (PUSH socket).
    auto publisher = std::make_unique<ZmqPublisher>(bind_addr);

    // Create TDLib client and wire up the callback.
    g_client = std::make_unique<TelegramClient>(
        api_id, api_hash, phone, channel_id);

    g_client->setMessageCallback(
        [&publisher](const std::string& text) {
            publisher->send(text);
        });

    // Graceful shutdown on SIGINT / SIGTERM.
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Run the blocking event loop.
    g_client->run();

    std::cerr << "[LISTENER][INFO] Shutting down.\n";
    return 0;
}
