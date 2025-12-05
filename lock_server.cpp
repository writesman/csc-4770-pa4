#include "LockManager.hpp"
#include "protocol.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <zmq.hpp>

LockManager lock_manager;

void worker_routine(zmq::context_t *ctx, std::string worker_id) {
  zmq::socket_t socket(*ctx, zmq::socket_type::dealer);
  socket.set(zmq::sockopt::routing_id, worker_id);
  socket.connect("inproc://backend");

  socket.send(zmq::buffer(Protocol::MSG_READY), zmq::send_flags::none);

  while (true) {
    zmq::message_t client_id, empty, payload;
    try {
      if (!socket.recv(client_id, zmq::recv_flags::none))
        break;
      socket.recv(empty, zmq::recv_flags::none);
      socket.recv(payload, zmq::recv_flags::none);
    } catch (...) {
      break;
    }

    std::stringstream ss(payload.to_string());
    std::string command, resource, mode;
    ss >> command >> resource >> mode;

    std::string reply_str = std::string(Protocol::MSG_ERROR);

    if (command == Protocol::CMD_LOCK) {
      lock_manager.acquire(resource, mode);
      reply_str = Protocol::MSG_OK;
    } else if (command == Protocol::CMD_UNLOCK) {
      lock_manager.release(resource, mode);
      reply_str = Protocol::MSG_OK;
    }

    socket.send(client_id, zmq::send_flags::sndmore);
    socket.send(empty, zmq::send_flags::sndmore);
    socket.send(zmq::buffer(reply_str), zmq::send_flags::none);
  }
}

int main() {
  zmq::context_t ctx(1);
  zmq::socket_t frontend(ctx, zmq::socket_type::router);
  zmq::socket_t backend(ctx, zmq::socket_type::router);

  frontend.bind("tcp://*:5555");
  backend.bind("inproc://backend");

  std::cout << "Server running on tcp://*:5555...\n";

  std::unordered_map<std::string, std::string> affinity_map;
  int worker_seq = 0;

  zmq::pollitem_t items[] = {{frontend, 0, ZMQ_POLLIN, 0},
                             {backend, 0, ZMQ_POLLIN, 0}};

  while (true) {
    zmq::poll(items, 2, -1);

    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t client_id, empty, payload;
      frontend.recv(client_id, zmq::recv_flags::none);
      frontend.recv(empty, zmq::recv_flags::none);
      frontend.recv(payload, zmq::recv_flags::none);

      std::string client_key = client_id.to_string();

      if (affinity_map.find(client_key) == affinity_map.end()) {
        std::string new_worker_id = "worker_" + std::to_string(++worker_seq);
        affinity_map[client_key] = new_worker_id;

        std::thread(worker_routine, &ctx, new_worker_id).detach();

        zmq::message_t w_id, w_ready;
        backend.recv(w_id, zmq::recv_flags::none);
        backend.recv(w_ready, zmq::recv_flags::none);
      }

      std::string target = affinity_map[client_key];
      backend.send(zmq::buffer(target), zmq::send_flags::sndmore);
      backend.send(client_id, zmq::send_flags::sndmore);
      backend.send(empty, zmq::send_flags::sndmore);
      backend.send(payload, zmq::send_flags::none);
    }

    if (items[1].revents & ZMQ_POLLIN) {
      zmq::message_t worker_id, client_id, empty, reply;
      backend.recv(worker_id, zmq::recv_flags::none);
      backend.recv(client_id, zmq::recv_flags::none);
      backend.recv(empty, zmq::recv_flags::none);
      backend.recv(reply, zmq::recv_flags::none);

      frontend.send(client_id, zmq::send_flags::sndmore);
      frontend.send(empty, zmq::send_flags::sndmore);
      frontend.send(reply, zmq::send_flags::none);
    }
  }
  return 0;
}