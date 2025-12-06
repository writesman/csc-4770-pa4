#include "session_registry.hpp"
#include <iostream>

int main() {
  zmq::context_t ctx(1);
  zmq::socket_t frontend(ctx, zmq::socket_type::router);
  zmq::socket_t backend(ctx, zmq::socket_type::router);

  frontend.bind("tcp://*:5555");
  backend.bind("inproc://backend");

  std::cout << "Lock Server started on tcp://*:5555" << std::endl;

  LockManager lock_manager;
  SessionRegistry sessions(&ctx, &lock_manager);

  zmq::pollitem_t items[] = {{frontend, 0, ZMQ_POLLIN, 0},
                             {backend, 0, ZMQ_POLLIN, 0}};

  while (true) {
    zmq::poll(items, 2, -1);

    // Client Request on Frontend
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t client_id, empty, payload;

      // Receive multi-part message: [Client ID | Empty | Payload]
      if (!frontend.recv(client_id, zmq::recv_flags::none))
        continue;
      frontend.recv(empty, zmq::recv_flags::none);
      frontend.recv(payload, zmq::recv_flags::none);

      std::string client_key = client_id.to_string();

      // Get worker ID (spawns thread if new client)
      std::string worker_id =
          sessions.get_worker_for_client(client_key, backend);

      // Forward to worker: [Worker ID | Client ID | Empty | Payload]
      backend.send(zmq::buffer(worker_id), zmq::send_flags::sndmore);
      backend.send(client_id, zmq::send_flags::sndmore);
      backend.send(empty, zmq::send_flags::sndmore);
      backend.send(payload, zmq::send_flags::none);
    }

    // Worker Reply on Backend
    if (items[1].revents & ZMQ_POLLIN) {
      zmq::message_t worker_id, client_id, empty, reply;

      // Receive reply frames: [Worker ID | Client ID | Empty | Reply]
      backend.recv(worker_id, zmq::recv_flags::none);
      backend.recv(client_id, zmq::recv_flags::none);
      backend.recv(empty, zmq::recv_flags::none);
      backend.recv(reply, zmq::recv_flags::none);

      // Route final reply back to client: [Client ID | Empty | Reply]
      frontend.send(client_id, zmq::send_flags::sndmore);
      frontend.send(empty, zmq::send_flags::sndmore);
      frontend.send(reply, zmq::send_flags::none);
    }
  }
  return 0;
}