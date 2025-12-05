#include "session_registry.hpp"
#include "protocol.hpp"
#include <iostream>
#include <thread>

// --- Worker Thread Logic ---
// This function executes the blocking lock management calls for a single client
// session.
void handle_worker_session(zmq::context_t *ctx, std::string session_worker_id,
                           LockManager *manager) {
  zmq::socket_t socket(
      *ctx, zmq::socket_type::dealer); // DEALER connects to the router backend
  socket.set(zmq::sockopt::routing_id, session_worker_id);
  socket.connect("inproc://backend");

  // Signal main thread that the worker is initialized and ready for routing.
  socket.send(zmq::buffer(Protocol::MSG_READY), zmq::send_flags::none);

  while (true) {
    zmq::message_t client_id, empty, payload;
    try {
      // Receive message from the main router [Client ID | Empty | Payload]
      if (!socket.recv(client_id, zmq::recv_flags::none))
        break;
      (void)socket.recv(empty, zmq::recv_flags::none);
      (void)socket.recv(payload, zmq::recv_flags::none);
    } catch (...) {
      break;
    }

    auto req = Protocol::LockRequest::parse(payload.to_string());
    std::string reply_str = std::string(Protocol::MSG_ERROR);

    // Execute the potentially blocking lock manager call.
    if (req.command == Protocol::CMD_LOCK) {
      manager->acquire(req.resource,
                       req.mode); // Blocks here until lock is secured
      reply_str = Protocol::MSG_OK;
    } else if (req.command == Protocol::CMD_UNLOCK) {
      manager->release(req.resource, req.mode);
      reply_str = Protocol::MSG_OK;
    }

    // Send reply back to the main router for forwarding: [Client ID | Empty |
    // Reply]
    socket.send(client_id, zmq::send_flags::sndmore);
    socket.send(empty, zmq::send_flags::sndmore);
    socket.send(zmq::buffer(reply_str), zmq::send_flags::none);
  }
}

// --- Session Registry Definitions ---
SessionRegistry::SessionRegistry(zmq::context_t *context, LockManager *mgr)
    : ctx(context), manager(mgr) {}

// Ensures every client is consistently handled by the same worker thread
// (affinity).
std::string SessionRegistry::get_worker_for_client(const std::string &client_id,
                                                   zmq::socket_t &backend) {
  // Check if affinity already exists.
  if (affinity_map.find(client_id) != affinity_map.end()) {
    return affinity_map.at(client_id);
  }

  // New Session: Assign ID and spawn thread.
  std::string new_session_worker_id = "worker_" + std::to_string(++worker_seq);
  affinity_map[client_id] = new_session_worker_id;

  // Launch worker thread (detached).
  std::thread(handle_worker_session, ctx, new_session_worker_id, manager)
      .detach();

  // Synchronization point: Block and wait for the new worker's READY signal
  // before routing traffic to it.
  zmq::message_t w_id, w_ready;
  (void)backend.recv(w_id, zmq::recv_flags::none);
  (void)backend.recv(w_ready, zmq::recv_flags::none);

  return new_session_worker_id;
}