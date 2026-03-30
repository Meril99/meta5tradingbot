// TelegramClient.hpp — TDLib wrapper for listening to a Telegram channel.
// Author: <your-name>
// Date:   <date>
#pragma once

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace td_api = td::td_api;

class TelegramClient {
public:
    // Construct with Telegram API credentials and target channel.
    TelegramClient(const std::string& api_id,
                   const std::string& api_hash,
                   const std::string& phone,
                   int64_t            channel_id);

    ~TelegramClient();

    // Register a callback invoked for every new message text from the channel.
    void setMessageCallback(std::function<void(const std::string&)> cb);

    // Blocking main loop — call from main(). Handles TDLib events until
    // stop() is called or the process receives SIGINT/SIGTERM.
    void run();

    // Request a graceful shutdown from another thread.
    void stop();

private:
    // Send a TDLib request and register a response handler.
    void send_query(td_api::object_ptr<td_api::Function> fn,
                    std::function<void(td_api::object_ptr<td_api::Object>)> handler);

    // Process a single TDLib response object.
    void process_response(td::ClientManager::Response response);

    // Handle the TDLib authorization state machine.
    void process_auth_state();

    // Handle incoming updateNewMessage.
    void on_new_message(td_api::object_ptr<td_api::updateNewMessage>& update);

    // Write heartbeat file for systemd watchdog.
    void write_heartbeat();

    std::unique_ptr<td::ClientManager> client_manager_;
    std::int32_t                        client_id_{0};

    std::string api_id_;
    std::string api_hash_;
    std::string phone_;
    int64_t     channel_id_;

    std::function<void(const std::string&)> message_cb_;

    std::atomic<bool>         running_{true};
    std::atomic<bool>         authorized_{false};
    td_api::object_ptr<td_api::AuthorizationState> auth_state_;

    std::uint64_t                                                      query_id_{0};
    std::map<std::uint64_t,
             std::function<void(td_api::object_ptr<td_api::Object>)>>  handlers_;
    std::mutex                                                         handlers_mu_;
};
