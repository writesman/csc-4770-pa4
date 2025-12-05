#ifndef LOCK_CLIENT_HPP
#define LOCK_CLIENT_HPP

#include "protocol.hpp"
#include <string>
#include <string_view>
#include <zmq.hpp>

class LockClient {
private:
  zmq::context_t ctx;
  zmq::socket_t sock;

public:
  LockClient() : ctx(1), sock(ctx, zmq::socket_type::req) {
    sock.connect("tcp://localhost:5555");
  }

  bool send_request(std::string_view cmd, std::string_view res,
                    std::string_view mode) {
    std::string msg =
        std::string(cmd) + " " + std::string(res) + " " + std::string(mode);
    sock.send(zmq::buffer(msg), zmq::send_flags::none);

    zmq::message_t reply;
    sock.recv(reply, zmq::recv_flags::none);

    return reply.to_string() == Protocol::MSG_OK;
  }
};

#endif