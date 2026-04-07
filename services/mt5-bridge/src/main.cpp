// main.cpp — Entry point for mt5-bridge service.
// Receives parsed trading signals via ZMQ PULL (port 5556) and trade events
// from the EA via ZMQ PULL (port 5557). Logs signals, sends Telegram
// notifications for trade events, and records everything to a CSV journal.
// Author: <your-name>
// Date:   <date>

#include "OrderReceiver.hpp"
#include "TelegramNotifier.hpp"
#include "TradeEvent.hpp"
#include "TradeEventReceiver.hpp"
#include "TradeJournal.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <zmq.h>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

std::string require_env(const char* name) {
    const char* val = std::getenv(name);
    if (!val || val[0] == '\0') {
        std::cerr << "[MT5-BRIDGE][FATAL] Missing env var: " << name << "\n";
        std::exit(1);
    }
    return val;
}

// Optional env var — returns empty string if not set.
std::string optional_env(const char* name) {
    const char* val = std::getenv(name);
    return (val && val[0] != '\0') ? std::string(val) : std::string();
}

// Current time as ISO-8601 string.
std::string now_iso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
    return buf;
}
}  // namespace

int main() {
    std::cerr << "[MT5-BRIDGE][INFO] Starting mt5-bridge\n";

    // ── Required: signal receiver ────────────────────────────────────────
    std::string parser_port = require_env("ZMQ_PARSER_PORT");
    std::string signal_addr = "tcp://127.0.0.1:" + parser_port;
    OrderReceiver signal_receiver(signal_addr);

    // ── Required: trade event receiver ───────────────────────────────────
    std::string event_port = require_env("ZMQ_EVENT_PORT");
    std::string event_addr = "tcp://0.0.0.0:" + event_port;
    TradeEventReceiver event_receiver(event_addr);

    // ── Optional: Telegram notifier (if bot token provided) ──────────────
    std::unique_ptr<TelegramNotifier> notifier;
    {
        std::string bot_token = optional_env("TELEGRAM_BOT_TOKEN");
        std::string chat_id   = optional_env("TELEGRAM_NOTIFY_CHAT_ID");
        if (!bot_token.empty() && !chat_id.empty()) {
            notifier = std::make_unique<TelegramNotifier>(bot_token, chat_id);
        } else {
            std::cerr << "[MT5-BRIDGE][WARN] TELEGRAM_BOT_TOKEN or "
                         "TELEGRAM_NOTIFY_CHAT_ID not set — notifications disabled\n";
        }
    }

    // ── Trade journal CSV ────────────────────────────────────────────────
    std::string journal_path = optional_env("TRADE_JOURNAL_PATH");
    if (journal_path.empty())
        journal_path = "/var/lib/telegram-mt5-bot/trade_journal.csv";
    TradeJournal journal(journal_path);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cerr << "[MT5-BRIDGE][INFO] Polling signals (" << signal_addr
              << ") and events (" << event_addr << ")\n";

    // ── Main poll loop ───────────────────────────────────────────────────
    // Use zmq_poll to watch both sockets with a 1 s timeout.
    zmq_pollitem_t items[2];
    items[0] = { signal_receiver.socket_handle(), 0, ZMQ_POLLIN, 0 };
    items[1] = { event_receiver.socket_handle(),  0, ZMQ_POLLIN, 0 };

    while (g_running) {
        int rc = zmq_poll(items, 2, 1000);  // 1 s timeout
        if (rc < 0) {
            if (zmq_errno() == EINTR) continue;  // signal interrupted
            std::cerr << "[MT5-BRIDGE][ERROR] zmq_poll: "
                      << zmq_strerror(zmq_errno()) << "\n";
            break;
        }

        // ── Handle parsed signal (port 5556) ─────────────────────────────
        if (items[0].revents & ZMQ_POLLIN) {
            std::string raw = signal_receiver.receive_raw();
            if (!raw.empty()) {
                std::string ts = now_iso();

                // Check for BREAKEVEN or CLOSEALL commands.
                if (raw.rfind("BREAKEVEN|", 0) == 0) {
                    std::string symbol = raw.substr(10);
                    std::cerr << "[SIGNAL][" << ts << "] BREAKEVEN " << symbol << "\n";

                    // Log to journal as a signal event.
                    TradeEvent be_ev;
                    be_ev.valid = true;
                    be_ev.type = "SIGNAL_BREAKEVEN";
                    be_ev.symbol = symbol;
                    be_ev.open_time = ts;
                    journal.record(be_ev);
                }
                else if (raw.rfind("CLOSEALL|", 0) == 0) {
                    std::string symbol = raw.substr(9);
                    std::cerr << "[SIGNAL][" << ts << "] CLOSEALL " << symbol << "\n";

                    TradeEvent cl_ev;
                    cl_ev.valid = true;
                    cl_ev.type = "SIGNAL_CLOSEALL";
                    cl_ev.symbol = symbol;
                    cl_ev.open_time = ts;
                    journal.record(cl_ev);
                }
                else {
                    ReceivedOrder order = OrderReceiver::deserialize(raw);
                    if (order.valid) {
                        std::cerr << "[SIGNAL][" << ts << "] ENTRY "
                                  << order.side << " " << order.symbol
                                  << " entry=" << order.entry
                                  << " sl=" << order.sl
                                  << " tp=" << order.tp1
                                  << " lots=" << order.lots << "\n";
                        OrderReceiver::write_debug_json(order);

                        TradeEvent entry_ev;
                        entry_ev.valid = true;
                        entry_ev.type = "SIGNAL_ENTRY";
                        entry_ev.symbol = order.symbol;
                        entry_ev.side = order.side;
                        entry_ev.open_price = order.entry;
                        entry_ev.sl = order.sl;
                        entry_ev.tp = order.tp1;
                        entry_ev.lots = order.lots;
                        entry_ev.open_time = ts;
                        journal.record(entry_ev);
                    }
                }
            }
        }

        // ── Handle trade event from EA (port 5557) ───────────────────────
        if (items[1].revents & ZMQ_POLLIN) {
            std::string raw = event_receiver.receive_raw();
            if (!raw.empty()) {
                TradeEvent ev = deserialize_trade_event(raw);
                if (ev.valid) {
                    std::cerr << "[EVENT][" << now_iso() << "] "
                              << ev.type << " " << ev.side
                              << " " << ev.symbol
                              << " ticket=#" << ev.ticket
                              << " P&L=$" << ev.profit << "\n";

                    // Record to CSV journal.
                    journal.record(ev);

                    // Send Telegram notification.
                    if (notifier) {
                        std::string msg =
                            TelegramNotifier::format_trade_event(ev);
                        notifier->send_message(msg);
                    }
                }
            }
        }
    }

    std::cerr << "[MT5-BRIDGE][INFO] Shutting down.\n";
    return 0;
}
