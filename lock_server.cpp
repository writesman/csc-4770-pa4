#include "protocol.hpp"
#include <condition_variable>
#include <format>
#include <mutex>
#include <print>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <zmq.hpp>

// --- LOCK MANAGER ---
// Keeps track of who owns what resource. Thread-safe.
enum LockMode { UNLOCKED, READ, WRITE };

struct ResourceState {
  LockMode mode = UNLOCKED;
  int active_readers = 0;
  std::condition_variable cv;
};

class LockManager {
private:
  std::unordered_map<std::string, ResourceState> resources;
  std::mutex global_mutex;

public:
  void acquire(const std::string &resource, const std::string &mode) {
    std::unique_lock<std::mutex> lock(global_mutex);

    // Ensure resource entry exists
    ResourceState &state = resources[resource];

    if (mode == Protocol::MODE_WRITE) {
      // Writers wait for EVERYTHING to be clear
      while (state.mode != UNLOCKED) {
        state.cv.wait(lock);
      }
      state.mode = WRITE;
    } else if (mode == Protocol::MODE_READ) {
      // Readers wait only if there is a WRITER
      while (state.mode == WRITE) {
        state.cv.wait(lock);
      }
      state.mode = READ;
      state.active_readers++;
    }
    std::println("[LOG] Locked {} in {} mode.", resource, mode);
  }

  void release(const std::string &resource) {
    std::unique_lock<std::mutex> lock(global_mutex);
    if (!resources.contains(resource))
      return;

    ResourceState &state = resources[resource];
    if (state.mode == WRITE) {
      state.mode = UNLOCKED;
    } else if (state.mode == READ) {
      state.active_readers--;
      if (state.active_readers == 0)
        state.mode = UNLOCKED;
    }

    std::println("[LOG] Released {}.", resource);

    // Wake up everyone waiting on this resource
    state.cv.notify_all();
  }
};

LockManager lock_manager;

// --- WORKER THREAD ---
// Handles logic for ONE specific client.
void worker_thread(zmq::context_t *ctx, std::string worker_identity) {
  zmq::socket_t socket(*ctx, zmq::socket_type::dealer);

  // IDENTITY MUST BE SET BEFORE CONNECTING
  socket.set(zmq::sockopt::routing_id, worker_identity);
  socket.connect("inproc://backend");

  zmq::message_t ready_msg(Protocol::MSG_READY.data(),
                           Protocol::MSG_READY.size());
  socket.send(ready_msg, zmq::send_flags::none);

  while (true) {
    // 1. Receive Request: [Client_ID][Empty][Payload]
    zmq::message_t client_id_msg, empty_msg, payload_msg;

    try {
      auto res = socket.recv(client_id_msg, zmq::recv_flags::none);
      if (!res)
        break; // Interrupted
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

    if (command == Protocol::CMD_LOCK) {
      lock_manager.acquire(resource, mode);
      reply_str = Protocol::MSG_OK;
    } else if (command == Protocol::CMD_UNLOCK) {
      lock_manager.release(resource);
      reply_str = Protocol::MSG_OK;
    }

    // 2. Send Reply: [Client_ID][Empty][Reply]
    socket.send(client_id_msg, zmq::send_flags::sndmore);
    socket.send(empty_msg, zmq::send_flags::sndmore);
    socket.send(zmq::buffer(reply_str), zmq::send_flags::none);
  }
}

// --- MAIN THREAD ---
// Async Router. Never blocks.
int main() {
  zmq::context_t ctx(1);

  // Frontend: TCP facing Clients
  zmq::socket_t frontend(ctx, zmq::socket_type::router);
  frontend.bind("tcp://*:5555");

  // Backend: In-process facing Workers
  zmq::socket_t backend(ctx, zmq::socket_type::router);
  backend.bind("inproc://backend");

  std::println("Lock Server started on tcp://*:5555");

  std::unordered_map<std::string, std::string> affinity;
  int worker_count = 0;

  zmq::pollitem_t items[] = {{frontend, 0, ZMQ_POLLIN, 0},
                             {backend, 0, ZMQ_POLLIN, 0}};

  while (true) {
    zmq::poll(items, 2, -1);

    // --- MSG FROM CLIENT ---
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t client_id, empty, payload;
      frontend.recv(client_id, zmq::recv_flags::none);
      frontend.recv(empty, zmq::recv_flags::none);
      frontend.recv(payload, zmq::recv_flags::none);

      std::string client_str = client_id.to_string();

      // If this is a new client, spawn a worker
      if (!affinity.contains(client_str)) {
        std::string worker_id = std::format("worker-{}", ++worker_count);
        affinity[client_str] = worker_id;

        std::thread(worker_thread, &ctx, worker_id).detach();
        std::println("Spawned {} for new client.", worker_id);

        zmq::message_t worker_id_frame, ready_msg;
        backend.recv(worker_id_frame, zmq::recv_flags::none);
        backend.recv(ready_msg, zmq::recv_flags::none);
      }

      std::string target_worker = affinity[client_str];

      // Forward to Backend: [Worker_ID][Client_ID][Empty][Payload]
      // Router removes [Worker_ID] and sends the rest to the Dealer.
      backend.send(zmq::buffer(target_worker), zmq::send_flags::sndmore);
      backend.send(client_id, zmq::send_flags::sndmore);
      backend.send(empty, zmq::send_flags::sndmore);
      backend.send(payload, zmq::send_flags::none);
    }

    // --- MSG FROM WORKER ---
    if (items[1].revents & ZMQ_POLLIN) {
      zmq::message_t worker_id, client_id, empty, reply;

      // Backend adds Worker_ID frame on receive
      backend.recv(worker_id, zmq::recv_flags::none);
      backend.recv(client_id, zmq::recv_flags::none);
      backend.recv(empty, zmq::recv_flags::none);
      backend.recv(reply, zmq::recv_flags::none);

      // Forward to Frontend: [Client_ID][Empty][Reply]
      frontend.send(client_id, zmq::send_flags::sndmore);
      frontend.send(empty, zmq::send_flags::sndmore);
      frontend.send(reply, zmq::send_flags::none);
    }
  }
  return 0;
}