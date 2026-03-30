// TradeEventReceiver.hpp — ZMQ PULL socket that receives trade events from the EA.
// Binds on a port (the EA connects as the transient side).
// Author: <your-name>
// Date:   <date>
#pragma once

#include <string>

class TradeEventReceiver {
public:
    // bind_addr: ZMQ PULL bind address (e.g. "tcp://0.0.0.0:5557")
    explicit TradeEventReceiver(const std::string& bind_addr);
    ~TradeEventReceiver();

    TradeEventReceiver(const TradeEventReceiver&) = delete;
    TradeEventReceiver& operator=(const TradeEventReceiver&) = delete;

    // Blocking receive with 1 s timeout. Returns empty string on timeout.
    std::string receive_raw();

    // Expose the raw ZMQ socket for zmq_poll().
    void* socket_handle() const { return sock_; }

private:
    void* ctx_;
    void* sock_;
};
