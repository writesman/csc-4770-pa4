// lock_server.cpp
#include "LockManager.hpp"
#include "protocol.hpp"
#include <format>
#include <print>
#include <sstream>
#include <string>
#include <thread>
#include <zmq.hpp>

// Global instance
LockManager lock_manager;

// --- WORKER THREAD ---
void worker_thread(zmq::context_t *ctx, std::string worker_identity) {
  zmq::socket_t socket(*ctx, zmq::socket_type::dealer);
  socket.set(zmq::sockopt::routing_id, worker_identity);
  socket.connect("inproc://backend");

  zmq::message_t ready_msg(Protocol::MSG_READY.data(),
                           Protocol::MSG_READY.size());
  socket.send(ready_msg, zmq::send_flags::none);

  while (true) {
    zmq::message_t client_id_msg, empty_msg, payload_msg;
    try {
      auto res = socket.recv(client_id_msg, zmq::recv_flags::none);
      if (!res)
        break;
      socket.recv(empty_msg, zmq::recv_flags::none);
      socket.recv(payload_msg, zmq::recv_flags::none);
    } catch (...) {
      break;
    }

    std::string req_str = payload_msg.to_string();
    std::stringstream ss(req_str);
    std::string command, resource, mode;
    ss >> command >> resource >> mode;

    std::string reply_str = std::string(Protocol::MSG_ERROR);

    // Simple If/Else matches the simple Protocol strings
    if (command == Protocol::CMD_LOCK) {
      lock_manager.acquire(resource, mode);
      reply_str = Protocol::MSG_OK;
    } else if (command == Protocol::CMD_UNLOCK) {
      lock_manager.release(resource);
      reply_str = Protocol::MSG_OK;
    }

    socket.send(client_id_msg, zmq::send_flags::sndmore);
    socket.send(empty_msg, zmq::send_flags::sndmore);
    socket.send(zmq::buffer(reply_str), zmq::send_flags::none);
  }
}

// --- MAIN THREAD ---
int main() {
  zmq::context_t ctx(1);

  zmq::socket_t frontend(ctx, zmq::socket_type::router);
  frontend.bind("tcp://*:5555");

  zmq::socket_t backend(ctx, zmq::socket_type::router);
  backend.bind("inproc://backend");

  std::println("Lock Server started on tcp://*:5555");

  std::unordered_map<std::string, std::string> affinity;
  int worker_count = 0;

  zmq::pollitem_t items[] = {{frontend, 0, ZMQ_POLLIN, 0},
                             {backend, 0, ZMQ_POLLIN, 0}};

  while (true) {
    zmq::poll(items, 2, -1);

    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t client_id, empty, payload;
      frontend.recv(client_id, zmq::recv_flags::none);
      frontend.recv(empty, zmq::recv_flags::none);
      frontend.recv(payload, zmq::recv_flags::none);

      std::string client_str = client_id.to_string();

      if (!affinity.contains(client_str)) {
        std::string worker_id = std::format("worker-{}", ++worker_count);
        affinity[client_str] = worker_id;
        std::thread(worker_thread, &ctx, worker_id).detach();

        zmq::message_t worker_id_frame, ready_msg;
        backend.recv(worker_id_frame, zmq::recv_flags::none);
        backend.recv(ready_msg, zmq::recv_flags::none);
      }

      std::string target_worker = affinity[client_str];
      backend.send(zmq::buffer(target_worker), zmq::send_flags::sndmore);
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