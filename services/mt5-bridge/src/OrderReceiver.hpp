// OrderReceiver.hpp — ZMQ PULL receiver for parsed trading signals.
// Deserializes pipe-delimited signals and logs them.
// Author: <your-name>
// Date:   <date>
#pragma once

#include <string>

// Mirrors the Signal struct from signal-parser (deserialized side).
struct ReceivedOrder {
    std::string symbol;
    std::string side;    // "BUY" or "SELL"
    double entry = 0.0;
    double sl    = 0.0;
    double tp1   = 0.0;
    double tp2   = 0.0;
    double lots  = 0.0;
    bool   valid = false;
};

class OrderReceiver {
public:
    // connect_addr: ZMQ PULL connect address (e.g. "tcp://127.0.0.1:5556")
    explicit OrderReceiver(const std::string& connect_addr);
    ~OrderReceiver();

    OrderReceiver(const OrderReceiver&) = delete;
    OrderReceiver& operator=(const OrderReceiver&) = delete;

    // Blocking receive with 1 s timeout. Returns empty string on timeout.
    std::string receive_raw();

    // Deserialize a pipe-delimited string into a ReceivedOrder.
    static ReceivedOrder deserialize(const std::string& raw);

    // Write an order to /tmp/last_signal.json for debugging.
    static void write_debug_json(const ReceivedOrder& order);

    // Expose the raw ZMQ socket for zmq_poll().
    void* socket_handle() const { return sock_; }

private:
    void* ctx_;
    void* sock_;
};
