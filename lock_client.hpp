#ifndef LOCK_CLIENT_HPP
#define LOCK_CLIENT_HPP

#include "protocol.hpp"
#include <string>
#include <zmq.hpp>

class LockClient {
private:
  zmq::context_t ctx;
  zmq::socket_t sock;

public:
  LockClient() : ctx(1), sock(ctx, zmq::socket_type::req) {
    sock.connect("tcp://localhost:5555");
  }

  bool send_request(const std::string &cmd, const std::string &res,
                    const std::string &mode) {
    Protocol::LockRequest req{cmd, res, mode};
    std::string msg = req.to_string();

    sock.send(zmq::buffer(msg), zmq::send_flags::none);

    zmq::message_t reply;
    (void)sock.recv(reply, zmq::recv_flags::none);

    return reply.to_string() == Protocol::MSG_OK;
  }
};

#endif