// ZmqPublisher.cpp — ZeroMQ PUSH socket implementation.
// Author: <your-name>
// Date:   <date>

#include "ZmqPublisher.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

ZmqPublisher::ZmqPublisher(const std::string& bind_addr) {
    ctx_ = zmq_ctx_new();
    if (!ctx_) {
        throw std::runtime_error("[ZMQ] Failed to create context");
    }

    sock_ = zmq_socket(ctx_, ZMQ_PUSH);
    if (!sock_) {
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error("[ZMQ] Failed to create PUSH socket");
    }

    int linger = 1000;  // 1 s linger on close
    zmq_setsockopt(sock_, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_bind(sock_, bind_addr.c_str()) != 0) {
        std::string err = "[ZMQ] Failed to bind to " + bind_addr +
                          ": " + zmq_strerror(zmq_errno());
        zmq_close(sock_);
        zmq_ctx_destroy(ctx_);
        throw std::runtime_error(err);
    }

    std::cerr << "[ZMQ][INFO] PUSH socket bound to " << bind_addr << "\n";
}

ZmqPublisher::~ZmqPublisher() {
    if (sock_) zmq_close(sock_);
    if (ctx_)  zmq_ctx_destroy(ctx_);
}

// Send a message as a single ZMQ frame.
void ZmqPublisher::send(const std::string& msg) {
    zmq_msg_t zmsg;
    zmq_msg_init_size(&zmsg, msg.size());
    std::memcpy(zmq_msg_data(&zmsg), msg.data(), msg.size());

    int rc = zmq_msg_send(&zmsg, sock_, 0);
    if (rc == -1) {
        std::cerr << "[ZMQ][ERROR] send failed: " << zmq_strerror(zmq_errno())
                  << "\n";
        zmq_msg_close(&zmsg);
        return;
    }
    zmq_msg_close(&zmsg);
}
