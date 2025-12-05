#include "session_registry.hpp"
#include <iostream>

int main() {
  zmq::context_t ctx(1);
  zmq::socket_t frontend(ctx, zmq::socket_type::router);
  zmq::socket_t backend(ctx, zmq::socket_type::router);

  frontend.bind("tcp://*:5555");
  backend.bind("inproc://backend");

  // EXACT REQUIRED OUTPUT
  std::cout << "Lock Server started on tcp://*:5555" << std::endl;

  LockManager lock_manager;
  SessionRegistry sessions(&ctx, &lock_manager);

  zmq::pollitem_t items[] = {{frontend, 0, ZMQ_POLLIN, 0},
                             {backend, 0, ZMQ_POLLIN, 0}};

  while (true) {
    zmq::poll(items, 2, -1);

    // Frontend: Route Client -> Worker
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t client_id, empty, payload;
      if (!frontend.recv(client_id, zmq::recv_flags::none))
        continue;
      (void)frontend.recv(empty, zmq::recv_flags::none);
      (void)frontend.recv(payload, zmq::recv_flags::none);

      std::string client_key = client_id.to_string();
      std::string worker_id =
          sessions.get_worker_for_client(client_key, backend);

      backend.send(zmq::buffer(worker_id), zmq::send_flags::sndmore);
      backend.send(client_id, zmq::send_flags::sndmore);
      backend.send(empty, zmq::send_flags::sndmore);
      backend.send(payload, zmq::send_flags::none);
    }

    // Backend: Route Worker -> Client
    if (items[1].revents & ZMQ_POLLIN) {
      zmq::message_t worker_id, client_id, empty, reply;
      (void)backend.recv(worker_id, zmq::recv_flags::none);
      (void)backend.recv(client_id, zmq::recv_flags::none);
      (void)backend.recv(empty, zmq::recv_flags::none);
      (void)backend.recv(reply, zmq::recv_flags::none);

      frontend.send(client_id, zmq::send_flags::sndmore);
      frontend.send(empty, zmq::send_flags::sndmore);
      frontend.send(reply, zmq::send_flags::none);
    }
  }
  return 0;
}