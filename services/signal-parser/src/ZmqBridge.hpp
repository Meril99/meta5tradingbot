// ZmqBridge.hpp — ZeroMQ PULL + PUSH bridge for the signal-parser service.
// Receives raw messages on the PULL side and forwards parsed signals on PUSH.
// Author: <your-name>
// Date:   <date>
#pragma once

#include <string>

class ZmqBridge {
public:
    // pull_addr: connect address for incoming messages  (e.g. "tcp://127.0.0.1:5555")
    // push_addr: bind address for outgoing parsed signals (e.g. "tcp://0.0.0.0:5556")
    ZmqBridge(const std::string& pull_addr, const std::string& push_addr);
    ~ZmqBridge();

    ZmqBridge(const ZmqBridge&) = delete;
    ZmqBridge& operator=(const ZmqBridge&) = delete;

    // Blocking receive — returns the raw message string (empty on timeout).
    std::string receive();

    // Send a serialized signal downstream.
    void send(const std::string& msg);

private:
    void* ctx_;
    void* pull_sock_;
    void* push_sock_;
};
