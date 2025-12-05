#include "session_registry.hpp"
#include <iostream>

// Main server loop responsible for ZeroMQ setup and routing client messages to
// worker threads.
int main() {
  zmq::context_t ctx(1);
  zmq::socket_t frontend(
      ctx, zmq::socket_type::router); // Public connection point (Clients)
  zmq::socket_t backend(
      ctx, zmq::socket_type::router); // Internal connection point (Workers)

  frontend.bind("tcp://*:5555");
  backend.bind("inproc://backend");

  // REQUIRED STARTUP OUTPUT
  std::cout << "Lock Server started on tcp://*:5555" << std::endl;

  LockManager lock_manager;
  // Instantiate registry to manage worker thread lifecycle and client affinity.
  SessionRegistry sessions(&ctx, &lock_manager);

  zmq::pollitem_t items[] = {
      {frontend, 0, ZMQ_POLLIN, 0}, // Check for incoming client messages
      {backend, 0, ZMQ_POLLIN, 0}   // Check for incoming worker replies/signals
  };

  while (true) {
    // Polls sockets to avoid blocking and manage concurrent events.
    zmq::poll(items, 2, -1);

    // --- FRONTEND ACTIVITY (Client Request) ---
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t client_id, empty, payload;

      // 1. Receive multi-part message [Client ID | Empty | Payload]
      if (!frontend.recv(client_id, zmq::recv_flags::none))
        continue;
      frontend.recv(empty, zmq::recv_flags::none);
      frontend.recv(payload, zmq::recv_flags::none);

      std::string client_key = client_id.to_string();

      // 2. Get worker ID (spawning thread if necessary for affinity)
      std::string worker_id =
          sessions.get_worker_for_client(client_key, backend);

      // 3. Forward message to the specific worker: [Worker ID | Client ID |
      // Empty | Payload]
      backend.send(zmq::buffer(worker_id), zmq::send_flags::sndmore);
      backend.send(client_id, zmq::send_flags::sndmore);
      backend.send(empty, zmq::send_flags::sndmore);
      backend.send(payload, zmq::send_flags::none);
    }

    // --- BACKEND ACTIVITY (Worker Reply) ---
    if (items[1].revents & ZMQ_POLLIN) {
      zmq::message_t worker_id, client_id, empty, reply;

      // 1. Receive reply frames: [Worker ID | Client ID | Empty | Reply]
      backend.recv(worker_id, zmq::recv_flags::none);
      backend.recv(client_id, zmq::recv_flags::none);
      backend.recv(empty, zmq::recv_flags::none);
      backend.recv(reply, zmq::recv_flags::none);

      // 2. Route final reply back to the client via frontend: [Client ID |
      // Empty | Reply]
      frontend.send(client_id, zmq::send_flags::sndmore);
      frontend.send(empty, zmq::send_flags::sndmore);
      frontend.send(reply, zmq::send_flags::none);
    }
  }
  return 0;
}