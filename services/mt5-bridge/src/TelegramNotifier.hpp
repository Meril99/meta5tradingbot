// TelegramNotifier.hpp — Sends Telegram Bot API messages for trade events.
// Uses libcurl for HTTPS POST to api.telegram.org.
// Author: <your-name>
// Date:   <date>
#pragma once

#include "TradeEvent.hpp"

#include <string>

class TelegramNotifier {
public:
    // bot_token: from @BotFather, e.g. "123456:ABC-DEF..."
    // chat_id:   user or group chat ID to send notifications to
    TelegramNotifier(const std::string& bot_token,
                     const std::string& chat_id);
    ~TelegramNotifier();

    TelegramNotifier(const TelegramNotifier&) = delete;
    TelegramNotifier& operator=(const TelegramNotifier&) = delete;

    // Send a raw text message (HTML parse mode).
    bool send_message(const std::string& text);

    // Format a TradeEvent into a human-readable HTML notification.
    static std::string format_trade_event(const TradeEvent& ev);

private:
    std::string api_url_;   // full URL: https://api.telegram.org/bot<token>/sendMessage
    std::string chat_id_;
    bool        curl_ok_;
};
