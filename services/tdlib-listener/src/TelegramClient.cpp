// TelegramClient.cpp — TDLib wrapper implementation.
// Author: <your-name>
// Date:   <date>

#include "TelegramClient.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <thread>

namespace {

// Helper: get current ISO-8601 timestamp string for log lines.
std::string now_iso() {
    auto t  = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
    return buf;
}

// Downcast helper for TDLib API objects.
template <typename T>
auto td_cast(td_api::object_ptr<td_api::Object>& obj) {
    return td_api::move_object_as<T>(obj);
}

}  // namespace

// ---------------------------------------------------------------------------
TelegramClient::TelegramClient(const std::string& api_id,
                               const std::string& api_hash,
                               const std::string& phone,
                               int64_t            channel_id)
    : api_id_(api_id),
      api_hash_(api_hash),
      phone_(phone),
      channel_id_(channel_id) {
    td::ClientManager::execute(
        td_api::make_object<td_api::setLogVerbosityLevel>(1));
    client_manager_ = std::make_unique<td::ClientManager>();
    client_id_      = client_manager_->create_client_id();
    // Kick-start TDLib by sending a no-op so we receive the auth state.
    send_query(td_api::make_object<td_api::getOption>("version"), {});
}

TelegramClient::~TelegramClient() {
    running_ = false;
}

void TelegramClient::setMessageCallback(
        std::function<void(const std::string&)> cb) {
    message_cb_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// Blocking main loop: poll TDLib for events and dispatch them.
// ---------------------------------------------------------------------------
void TelegramClient::run() {
    auto last_heartbeat = std::chrono::steady_clock::now();

    while (running_) {
        auto response = client_manager_->receive(1.0);  // 1 s timeout
        if (response.object) {
            process_response(std::move(response));
        }

        // Write heartbeat every 30 s so the systemd watchdog knows we're alive.
        auto elapsed = std::chrono::steady_clock::now() - last_heartbeat;
        if (elapsed > std::chrono::seconds(30)) {
            write_heartbeat();
            last_heartbeat = std::chrono::steady_clock::now();
        }
    }
}

void TelegramClient::stop() {
    running_ = false;
}

// ---------------------------------------------------------------------------
// Internal: send a TDLib request and optionally register a handler.
// ---------------------------------------------------------------------------
void TelegramClient::send_query(
        td_api::object_ptr<td_api::Function> fn,
        std::function<void(td_api::object_ptr<td_api::Object>)> handler) {
    auto id = ++query_id_;
    if (handler) {
        std::lock_guard<std::mutex> lk(handlers_mu_);
        handlers_.emplace(id, std::move(handler));
    }
    client_manager_->send(client_id_, id, std::move(fn));
}

// ---------------------------------------------------------------------------
// Dispatch a TDLib response: either to a registered handler or to the
// authorization / message update code paths.
// ---------------------------------------------------------------------------
void TelegramClient::process_response(
        td::ClientManager::Response response) {
    // If this response matches a pending query handler, invoke it.
    if (response.request_id != 0) {
        std::function<void(td_api::object_ptr<td_api::Object>)> handler;
        {
            std::lock_guard<std::mutex> lk(handlers_mu_);
            auto it = handlers_.find(response.request_id);
            if (it != handlers_.end()) {
                handler = std::move(it->second);
                handlers_.erase(it);
            }
        }
        if (handler) {
            handler(std::move(response.object));
        }
        return;
    }

    // Otherwise it is an unsolicited update.
    auto update_id = response.object->get_id();

    if (update_id == td_api::updateAuthorizationState::ID) {
        auto update = td_cast<td_api::updateAuthorizationState>(response.object);
        auth_state_ = std::move(update->authorization_state_);
        process_auth_state();
        return;
    }

    if (update_id == td_api::updateNewMessage::ID) {
        auto update = td_cast<td_api::updateNewMessage>(response.object);
        on_new_message(update);
        return;
    }
}

// ---------------------------------------------------------------------------
// Handle the TDLib authorization state machine. Sends credentials as needed.
// ---------------------------------------------------------------------------
void TelegramClient::process_auth_state() {
    if (!auth_state_) return;

    switch (auth_state_->get_id()) {
    case td_api::authorizationStateWaitTdlibParameters::ID: {
        auto params = td_api::make_object<td_api::setTdlibParameters>();
        params->use_test_dc_              = false;
        params->database_directory_       = "tdlib_session";
        params->files_directory_          = "";
        params->database_encryption_key_  = "";
        params->use_file_database_        = true;
        params->use_chat_info_database_   = true;
        params->use_message_database_     = true;
        params->use_secret_chats_         = false;
        params->api_id_                   = std::stoi(api_id_);
        params->api_hash_                 = api_hash_;
        params->system_language_code_     = "en";
        params->device_model_             = "Server";
        params->system_version_           = "";
        params->application_version_      = "1.0";
        send_query(std::move(params), [](auto obj) {
            if (obj->get_id() == td_api::error::ID) {
                auto err = td_cast<td_api::error>(obj);
                std::cerr << "[TDLIB][ERROR] setTdlibParameters failed: "
                          << err->message_ << "\n";
            }
        });
        break;
    }

    case td_api::authorizationStateWaitPhoneNumber::ID:
        std::cerr << "[TDLIB][INFO] Sending phone number for auth\n";
        send_query(
            td_api::make_object<td_api::setAuthenticationPhoneNumber>(
                phone_, nullptr),
            [](auto obj) {
                if (obj->get_id() == td_api::error::ID) {
                    auto err = td_cast<td_api::error>(obj);
                    std::cerr << "[TDLIB][ERROR] Phone auth failed: "
                              << err->message_ << "\n";
                }
            });
        break;

    case td_api::authorizationStateWaitCode::ID: {
        // Interactive: prompt from stdin (first-time auth only).
        std::cerr << "[TDLIB][INFO] Enter Telegram auth code: ";
        std::string code;
        std::getline(std::cin, code);
        send_query(
            td_api::make_object<td_api::checkAuthenticationCode>(code),
            [](auto obj) {
                if (obj->get_id() == td_api::error::ID) {
                    auto err = td_cast<td_api::error>(obj);
                    std::cerr << "[TDLIB][ERROR] Code check failed: "
                              << err->message_ << "\n";
                }
            });
        break;
    }

    case td_api::authorizationStateWaitPassword::ID: {
        std::cerr << "[TDLIB][INFO] Enter 2FA password: ";
        std::string password;
        std::getline(std::cin, password);
        send_query(
            td_api::make_object<td_api::checkAuthenticationPassword>(password),
            [](auto obj) {
                if (obj->get_id() == td_api::error::ID) {
                    auto err = td_cast<td_api::error>(obj);
                    std::cerr << "[TDLIB][ERROR] Password check failed: "
                              << err->message_ << "\n";
                }
            });
        break;
    }

    case td_api::authorizationStateReady::ID:
        authorized_ = true;
        std::cerr << "[TDLIB][INFO] Authorization complete. Listening for "
                     "messages on channel " << channel_id_ << "\n";
        break;

    case td_api::authorizationStateClosed::ID:
        std::cerr << "[TDLIB][INFO] TDLib session closed.\n";
        running_ = false;
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Handle an incoming message: filter by channel ID, extract text, forward.
// ---------------------------------------------------------------------------
void TelegramClient::on_new_message(
        td_api::object_ptr<td_api::updateNewMessage>& update) {
    if (!update || !update->message_) return;

    auto& msg = update->message_;
    int64_t chat_id = msg->chat_id_;

    // Only forward messages from the configured channel.
    if (chat_id != channel_id_) return;

    // Extract plain text content.
    if (!msg->content_) return;
    if (msg->content_->get_id() != td_api::messageText::ID) return;

    auto& text_content =
        static_cast<td_api::messageText&>(*msg->content_);
    if (!text_content.text_) return;

    const std::string& text = text_content.text_->text_;
    if (text.empty()) return;

    std::cerr << "[TDLIB][MSG " << now_iso() << "] "
              << text.substr(0, 120) << "\n";

    if (message_cb_) {
        message_cb_(text);
    }
}

// ---------------------------------------------------------------------------
// Write a heartbeat file so the systemd watchdog can verify liveness.
// ---------------------------------------------------------------------------
void TelegramClient::write_heartbeat() {
    std::ofstream f("/tmp/tdlib-listener.heartbeat");
    if (f) {
        f << now_iso() << "\n";
    }
}
