// ZmqBridge.cpp — PULL/PUSH bridge implementation.
// Author: <your-name>
// Date:   <date>

#include "ZmqBridge.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <zmq.h>

ZmqBridge::ZmqBridge(const std::string& pull_addr,
                     const std::string& push_addr) {
    ctx_ = zmq_ctx_new();
    if (!ctx_)
        throw std::runtime_error("[ZMQ] Failed to create context");

    // ── PULL socket (connect to listener's PUSH) ─────────────────────────
    pull_sock_ = zmq_socket(ctx_, ZMQ_PULL);
    if (!pull_sock_) {
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error("[ZMQ] Failed to create PULL socket");
    }

    int timeout_ms = 1000;  // 1 s receive timeout for graceful shutdown checks
    zmq_setsockopt(pull_sock_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    if (zmq_connect(pull_sock_, pull_addr.c_str()) != 0) {
        std::string err = "[ZMQ] PULL connect failed: " +
                          std::string(zmq_strerror(zmq_errno()));
        zmq_close(pull_sock_);
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error(err);
    }
    std::cerr << "[ZMQ][INFO] PULL connected to " << pull_addr << "\n";

    // ── PUSH socket (bind for mt5-bridge to connect) ─────────────────────
    push_sock_ = zmq_socket(ctx_, ZMQ_PUSH);
    if (!push_sock_) {
        zmq_close(pull_sock_);
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error("[ZMQ] Failed to create PUSH socket");
    }

    int linger = 1000;
    zmq_setsockopt(push_sock_, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_bind(push_sock_, push_addr.c_str()) != 0) {
        std::string err = "[ZMQ] PUSH bind failed: " +
                          std::string(zmq_strerror(zmq_errno()));
        zmq_close(push_sock_);
        zmq_close(pull_sock_);
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error(err);
    }
    std::cerr << "[ZMQ][INFO] PUSH bound to " << push_addr << "\n";
}

ZmqBridge::~ZmqBridge() {
    if (push_sock_) zmq_close(push_sock_);
    if (pull_sock_) zmq_close(pull_sock_);
    if (ctx_)       zmq_ctx_destroy(ctx_);
}

// Blocking receive with timeout. Returns empty string on timeout.
std::string ZmqBridge::receive() {
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    int rc = zmq_msg_recv(&msg, pull_sock_, 0);
    if (rc == -1) {
        zmq_msg_close(&msg);
        return {};  // timeout or error
    }

    std::string result(static_cast<char*>(zmq_msg_data(&msg)),
                       zmq_msg_size(&msg));
    zmq_msg_close(&msg);
    return result;
}

// Send a string on the PUSH socket.
void ZmqBridge::send(const std::string& msg) {
    zmq_msg_t zmsg;
    zmq_msg_init_size(&zmsg, msg.size());
    std::memcpy(zmq_msg_data(&zmsg), msg.data(), msg.size());

    int rc = zmq_msg_send(&zmsg, push_sock_, 0);
    if (rc == -1) {
        std::cerr << "[ZMQ][ERROR] PUSH send failed: "
                  << zmq_strerror(zmq_errno()) << "\n";
    }
    zmq_msg_close(&zmsg);
}
