#include "session_registry.hpp"
#include "protocol.hpp"
#include <iostream>
#include <thread>

// --- WORKER LOGIC DEFINITION ---
void worker_task(zmq::context_t *ctx, std::string worker_id,
                 LockManager *manager) {
  zmq::socket_t socket(*ctx, zmq::socket_type::dealer);
  socket.set(zmq::sockopt::routing_id, worker_id);
  socket.connect("inproc://backend");

  socket.send(zmq::buffer(Protocol::MSG_READY), zmq::send_flags::none);

  while (true) {
    zmq::message_t client_id, empty, payload;
    try {
      if (!socket.recv(client_id, zmq::recv_flags::none))
        break;
      (void)socket.recv(empty, zmq::recv_flags::none);
      (void)socket.recv(payload, zmq::recv_flags::none);
    } catch (...) {
      break;
    }

    auto req = Protocol::LockRequest::parse(payload.to_string());
    std::string reply_str = std::string(Protocol::MSG_ERROR);

    if (req.command == Protocol::CMD_LOCK) {
      manager->acquire(req.resource, req.mode);
      reply_str = Protocol::MSG_OK;
    } else if (req.command == Protocol::CMD_UNLOCK) {
      manager->release(req.resource, req.mode);
      reply_str = Protocol::MSG_OK;
    }

    socket.send(client_id, zmq::send_flags::sndmore);
    socket.send(empty, zmq::send_flags::sndmore);
    socket.send(zmq::buffer(reply_str), zmq::send_flags::none);
  }
}

// --- SESSION REGISTRY DEFINITIONS ---
SessionRegistry::SessionRegistry(zmq::context_t *context, LockManager *mgr)
    : ctx(context), manager(mgr) {}

std::string SessionRegistry::get_worker_for_client(const std::string &client_id,
                                                   zmq::socket_t &backend) {
  if (affinity_map.count(client_id)) {
    return affinity_map[client_id];
  }

  std::string new_worker_id = "worker_" + std::to_string(++worker_seq);
  affinity_map[client_id] = new_worker_id;

  std::thread(worker_task, ctx, new_worker_id, manager).detach();

  zmq::message_t w_id, w_ready;
  (void)backend.recv(w_id, zmq::recv_flags::none);
  (void)backend.recv(w_ready, zmq::recv_flags::none);

  return new_worker_id;
}