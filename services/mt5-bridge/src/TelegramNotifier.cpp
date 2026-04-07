// TelegramNotifier.cpp — Telegram Bot API notification sender via libcurl.
// Author: <your-name>
// Date:   <date>

#include "TelegramNotifier.hpp"

#include <cmath>
#include <curl/curl.h>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

// libcurl write callback — discard the response body.
size_t discard_body(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

// URL-encode a string for POST form data.
std::string url_encode(CURL* curl, const std::string& s) {
    char* encoded = curl_easy_escape(curl, s.c_str(), static_cast<int>(s.size()));
    std::string result(encoded);
    curl_free(encoded);
    return result;
}

// Format seconds into human-readable duration: "2h 34m" or "45m" or "12s".
std::string format_duration(int64_t secs) {
    if (secs < 0) secs = 0;
    int64_t h = secs / 3600;
    int64_t m = (secs % 3600) / 60;
    int64_t s = secs % 60;

    std::ostringstream os;
    if (h > 0) os << h << "h ";
    if (h > 0 || m > 0) os << m << "m";
    if (h == 0 && m == 0) os << s << "s";
    return os.str();
}

}  // namespace

// ---------------------------------------------------------------------------
TelegramNotifier::TelegramNotifier(const std::string& bot_token,
                                   const std::string& chat_id)
    : chat_id_(chat_id), curl_ok_(false) {
    api_url_ = "https://api.telegram.org/bot" + bot_token + "/sendMessage";

    CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (rc != CURLE_OK) {
        std::cerr << "[NOTIFY][ERROR] curl_global_init failed: "
                  << curl_easy_strerror(rc) << "\n";
    } else {
        curl_ok_ = true;
        std::cerr << "[NOTIFY][INFO] Telegram notifier ready (chat "
                  << chat_id_ << ")\n";
    }
}

TelegramNotifier::~TelegramNotifier() {
    if (curl_ok_) curl_global_cleanup();
}

// ---------------------------------------------------------------------------
// Send an HTML-formatted message via the Bot API.
// Returns true on success, false on any error.
// ---------------------------------------------------------------------------
bool TelegramNotifier::send_message(const std::string& text) {
    if (!curl_ok_) {
        std::cerr << "[NOTIFY][ERROR] curl not initialized\n";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[NOTIFY][ERROR] curl_easy_init failed\n";
        return false;
    }

    // Build POST body: chat_id=...&text=...&parse_mode=HTML
    std::string post_data =
        "chat_id=" + url_encode(curl, chat_id_) +
        "&text=" + url_encode(curl, text) +
        "&parse_mode=HTML";

    curl_easy_setopt(curl, CURLOPT_URL, api_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    bool ok = true;

    if (res != CURLE_OK) {
        std::cerr << "[NOTIFY][ERROR] HTTP request failed: "
                  << curl_easy_strerror(res) << "\n";
        ok = false;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200) {
            std::cerr << "[NOTIFY][WARN] Telegram API returned HTTP "
                      << http_code << "\n";
            ok = false;
        }
    }

    curl_easy_cleanup(curl);
    return ok;
}

// ---------------------------------------------------------------------------
// Format a trade event into a human-readable Telegram message with emojis
// and key metrics: P&L, pips, duration, prices.
// ---------------------------------------------------------------------------
std::string TelegramNotifier::format_trade_event(const TradeEvent& ev) {
    std::ostringstream os;
    os << std::fixed;

    // Determine pip size for this symbol (simplified lookup, mirrors parser).
    double pip_sz = 0.0001;  // default forex
    if (ev.symbol.find("JPY") != std::string::npos) pip_sz = 0.01;
    if (ev.symbol.find("XAU") != std::string::npos) pip_sz = 0.10;
    if (ev.symbol.find("XAG") != std::string::npos) pip_sz = 0.001;
    if (ev.symbol == "US30")    pip_sz = 1.0;
    if (ev.symbol == "NASDAQ" || ev.symbol == "NAS100") pip_sz = 0.01;
    if (ev.symbol.find("OIL") != std::string::npos) pip_sz = 0.01;

    double pips_moved = ev.pips(pip_sz);

    // ── Choose emoji and header based on event type ──────────────────────
    if (ev.type == "OPENED") {
        os << "\xF0\x9F\x93\xA5 <b>TRADE OPENED</b>\n\n";
        os << (ev.side == "BUY" ? "\xF0\x9F\x9F\xA2" : "\xF0\x9F\x94\xB4")
           << " " << ev.side << " <b>" << ev.symbol << "</b>\n";
        os << std::setprecision(5);
        os << "Entry: " << ev.open_price << "\n";
        os << "SL: " << ev.sl << " | TP: " << ev.tp << "\n";
        os << std::setprecision(2);
        os << "Lots: " << ev.lots << "\n";
        os << "Ticket: #" << ev.ticket << "\n";
        if (!ev.open_time.empty())
            os << "Time: " << ev.open_time;
    }
    else if (ev.type == "TP_HIT") {
        os << "\xF0\x9F\x8E\xAF <b>TAKE PROFIT HIT</b>\n\n";
        os << (ev.side == "BUY" ? "\xF0\x9F\x9F\xA2" : "\xF0\x9F\x94\xB4")
           << " " << ev.side << " <b>" << ev.symbol << "</b>\n";
        os << std::setprecision(5);
        os << "Entry: " << ev.open_price
           << " \xe2\x86\x92 Exit: " << ev.close_price << "\n";
        os << std::setprecision(2);
        os << "\xe2\x9c\x85 <b>Profit: +$" << std::abs(ev.profit)
           << "</b> (+" << std::setprecision(1) << pips_moved << " pips)\n";
        os << "Lots: " << std::setprecision(2) << ev.lots << "\n";
        os << "\xe2\x8f\xb1 Duration: " << format_duration(ev.duration_secs) << "\n";
        os << "Ticket: #" << ev.ticket;
    }
    else if (ev.type == "SL_HIT") {
        os << "\xF0\x9F\x9B\x91 <b>STOP LOSS HIT</b>\n\n";
        os << (ev.side == "BUY" ? "\xF0\x9F\x9F\xA2" : "\xF0\x9F\x94\xB4")
           << " " << ev.side << " <b>" << ev.symbol << "</b>\n";
        os << std::setprecision(5);
        os << "Entry: " << ev.open_price
           << " \xe2\x86\x92 Exit: " << ev.close_price << "\n";
        os << std::setprecision(2);
        os << "\xe2\x9d\x8c <b>Loss: -$" << std::abs(ev.profit)
           << "</b> (-" << std::setprecision(1) << pips_moved << " pips)\n";
        os << "Lots: " << std::setprecision(2) << ev.lots << "\n";
        os << "\xe2\x8f\xb1 Duration: " << format_duration(ev.duration_secs) << "\n";
        os << "Ticket: #" << ev.ticket;
    }
    else if (ev.type == "BREAKEVEN") {
        os << "\xF0\x9F\x94\x84 <b>BREAK EVEN</b>\n\n";
        os << "<b>" << ev.symbol << "</b>\n";
        os << "SL moved to entry on " << ev.ticket << " position(s)\n";
        if (!ev.open_time.empty())
            os << "Time: " << ev.open_time;
    }
    else if (ev.type == "CLOSEALL") {
        os << "\xF0\x9F\x9A\xAA <b>ALL POSITIONS CLOSED</b>\n\n";
        os << "<b>" << ev.symbol << "</b>\n";
        os << "Closed " << ev.ticket << " position(s)\n";
        os << std::setprecision(2);
        os << "Total P&L: <b>$" << ev.profit << "</b>\n";
        if (!ev.open_time.empty())
            os << "Time: " << ev.open_time;
    }
    else if (ev.type == "CLOSED") {
        // Manual close or unknown reason.
        bool is_profit = ev.profit >= 0.0;
        if (is_profit)
            os << "\xe2\x9c\x85 <b>TRADE CLOSED (Profit)</b>\n\n";
        else
            os << "\xe2\x9d\x8c <b>TRADE CLOSED (Loss)</b>\n\n";

        os << (ev.side == "BUY" ? "\xF0\x9F\x9F\xA2" : "\xF0\x9F\x94\xB4")
           << " " << ev.side << " <b>" << ev.symbol << "</b>\n";
        os << std::setprecision(5);
        os << "Entry: " << ev.open_price
           << " \xe2\x86\x92 Exit: " << ev.close_price << "\n";
        os << std::setprecision(2);
        if (is_profit)
            os << "<b>Profit: +$" << std::abs(ev.profit) << "</b>";
        else
            os << "<b>Loss: -$" << std::abs(ev.profit) << "</b>";
        os << " (" << (is_profit ? "+" : "-")
           << std::setprecision(1) << pips_moved << " pips)\n";
        os << "Lots: " << std::setprecision(2) << ev.lots << "\n";
        os << "\xe2\x8f\xb1 Duration: " << format_duration(ev.duration_secs) << "\n";
        os << "Ticket: #" << ev.ticket;
    }
    else {
        // Unknown event type — just dump what we have.
        os << "\xe2\x84\xb9\xef\xb8\x8f <b>" << ev.type << "</b> "
           << ev.side << " " << ev.symbol
           << " ticket=#" << ev.ticket << "\n";
        os << std::setprecision(2);
        os << "P&L: $" << ev.profit;
    }

    // Append spread info for context.
    if (ev.spread > 0.0) {
        os << "\nSpread: " << std::setprecision(1) << ev.spread << " pts";
    }

    return os.str();
}
