// OrderReceiver.cpp — ZMQ PULL receiver and signal deserializer.
// Author: <your-name>
// Date:   <date>

#include "OrderReceiver.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <zmq.h>

OrderReceiver::OrderReceiver(const std::string& connect_addr) {
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

    if (zmq_connect(sock_, connect_addr.c_str()) != 0) {
        std::string err = "[ZMQ] PULL connect failed: " +
                          std::string(zmq_strerror(zmq_errno()));
        zmq_close(sock_);
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error(err);
    }

    std::cerr << "[MT5-BRIDGE][INFO] PULL connected to "
              << connect_addr << "\n";
}

OrderReceiver::~OrderReceiver() {
    if (sock_) zmq_close(sock_);
    if (ctx_)  zmq_ctx_destroy(ctx_);
}

std::string OrderReceiver::receive_raw() {
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

// Parse pipe-delimited string: SYMBOL|SIDE|entry|sl|tp1|tp2|lots
ReceivedOrder OrderReceiver::deserialize(const std::string& raw) {
    ReceivedOrder order;
    std::vector<std::string> parts;
    std::istringstream ss(raw);
    std::string token;

    while (std::getline(ss, token, '|')) {
        parts.push_back(token);
    }

    if (parts.size() < 7) {
        std::cerr << "[MT5-BRIDGE][ERROR] Malformed signal: " << raw << "\n";
        return order;
    }

    order.symbol = parts[0];
    order.side   = parts[1];
    order.entry  = std::stod(parts[2]);
    order.sl     = std::stod(parts[3]);
    order.tp1    = std::stod(parts[4]);
    order.tp2    = std::stod(parts[5]);
    order.lots   = std::stod(parts[6]);
    order.valid  = true;

    return order;
}

// Write the most recent signal to /tmp/last_signal.json for debugging.
void OrderReceiver::write_debug_json(const ReceivedOrder& order) {
    std::ofstream f("/tmp/last_signal.json");
    if (!f) {
        std::cerr << "[MT5-BRIDGE][WARN] Could not write /tmp/last_signal.json\n";
        return;
    }
    f << "{\n"
      << "  \"symbol\": \"" << order.symbol << "\",\n"
      << "  \"side\":   \"" << order.side   << "\",\n"
      << "  \"entry\":  " << order.entry  << ",\n"
      << "  \"sl\":     " << order.sl     << ",\n"
      << "  \"tp1\":    " << order.tp1    << ",\n"
      << "  \"tp2\":    " << order.tp2    << ",\n"
      << "  \"lots\":   " << order.lots   << "\n"
      << "}\n";
}
