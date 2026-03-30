// ZmqPublisher.hpp — ZeroMQ PUSH socket wrapper for forwarding raw messages.
// Author: <your-name>
// Date:   <date>
#pragma once

#include <zmq.h>
#include <memory>
#include <string>

class ZmqPublisher {
public:
    // bind_addr example: "tcp://0.0.0.0:5555"
    explicit ZmqPublisher(const std::string& bind_addr);
    ~ZmqPublisher();

    ZmqPublisher(const ZmqPublisher&) = delete;
    ZmqPublisher& operator=(const ZmqPublisher&) = delete;

    // Send a UTF-8 string over the PUSH socket (blocking).
    void send(const std::string& msg);

private:
    void* ctx_;
    void* sock_;
};
