// TradeEventReceiver.cpp — ZMQ PULL socket that binds and receives trade events.
// Author: <your-name>
// Date:   <date>

#include "TradeEventReceiver.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <zmq.h>

TradeEventReceiver::TradeEventReceiver(const std::string& bind_addr) {
    ctx_ = zmq_ctx_new();
    if (!ctx_)
        throw std::runtime_error("[ZMQ] Failed to create context");

    sock_ = zmq_socket(ctx_, ZMQ_PULL);
    if (!sock_) {
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error("[ZMQ] Failed to create PULL socket");
    }

    int timeout_ms = 1000;
    zmq_setsockopt(sock_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    if (zmq_bind(sock_, bind_addr.c_str()) != 0) {
        std::string err = "[ZMQ] PULL bind failed on " + bind_addr +
                          ": " + zmq_strerror(zmq_errno());
        zmq_close(sock_);
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error(err);
    }

    std::cerr << "[MT5-BRIDGE][INFO] Trade event PULL bound to "
              << bind_addr << "\n";
}

TradeEventReceiver::~TradeEventReceiver() {
    if (sock_) zmq_close(sock_);
    if (ctx_)  zmq_ctx_destroy(ctx_);
}

std::string TradeEventReceiver::receive_raw() {
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    int rc = zmq_msg_recv(&msg, sock_, 0);
    if (rc == -1) {
        zmq_msg_close(&msg);
        return {};
    }

    std::string result(static_cast<char*>(zmq_msg_data(&msg)),
                       zmq_msg_size(&msg));
    zmq_msg_close(&msg);
    return result;
}
