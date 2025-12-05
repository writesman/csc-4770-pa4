// lock_server.cpp
// Build: g++ -std=c++17 -O2 -pthread lock_server.cpp -lzmq -o lock_server

#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <zmq.hpp>

struct LockState {
  bool locked = false;
  std::string owner;
  std::deque<std::string> waiters;
  std::string value;
};

std::unordered_map<std::string, LockState> lock_table;
std::mutex table_mtx;

std::deque<std::pair<std::string, std::string>> response_queue;
std::mutex queue_mtx;

void enqueue_response(const std::string &client_id,
                      const std::string &payload) {
  std::lock_guard<std::mutex> lk(queue_mtx);
  response_queue.push_back({client_id, payload});
}

std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(' ');

  if (a == std::string::npos)
    return "";

  size_t b = s.find_last_not_of(' ');

  return s.substr(a, b - a + 1);
}

struct WorkerInfo {
  std::string client_id;
  zmq::socket_t socket;
};

void worker_thread(zmq::context_t *ctx, std::string endpoint,
                   std::string client_id) {
  zmq::socket_t pair(*ctx, zmq::socket_type::pair);
  pair.connect(endpoint);

  while (true) {
    zmq::message_t msg;
    // Receive request from Main thread (forwarded from Client)
    pair.recv(msg, zmq::recv_flags::none);
    std::string payload(static_cast<char *>(msg.data()), msg.size());

    // Parse command
    std::istringstream iss(payload);
    std::string cmd;
    iss >> cmd;

    if (cmd.empty()) {
      enqueue_response(client_id, "ERROR empty_command");
    } else if (cmd == "ACQUIRE") {
      std::string resource;
      iss >> resource;

      if (resource.empty()) {
        enqueue_response(client_id, "ERROR missing_resource");
      } else {
        std::unique_lock<std::mutex> lk(table_mtx);
        auto &ls = lock_table[resource];

        if (!ls.locked) {
          ls.locked = true;
          ls.owner = client_id;
          enqueue_response(client_id, "GRANTED " + resource);
        } else {
          // Blocked: Add to waiters. Do NOT send a response yet.
          ls.waiters.push_back(client_id);
          std::cerr << "Queued client [" << client_id << "] for " << resource
                    << "\n";
        }
      }
    } else if (cmd == "RELEASE") {
      std::string resource;
      iss >> resource;
      std::unique_lock<std::mutex> lk(table_mtx);
      auto it = lock_table.find(resource);

      if (it == lock_table.end() || !it->second.locked ||
          it->second.owner != client_id) {
        // Not locked or not owner -> OK (idempotent)
        enqueue_response(client_id, "OK " + resource);
      } else {
        // Release lock
        LockState &ls = it->second;

        if (!ls.waiters.empty()) {
          // Hand over to next waiter
          std::string next_client = ls.waiters.front();
          ls.waiters.pop_front();
          ls.owner = next_client;
          // Notify the RELEASER
          enqueue_response(client_id, "OK " + resource);
          // Notify the WAITER (Async)
          enqueue_response(next_client, "GRANTED " + resource);
        } else {
          // Fully unlock
          ls.locked = false;
          ls.owner.clear();
          enqueue_response(client_id, "OK " + resource);
        }
      }
    } else if (cmd == "WRITE") {
      std::string resource;
      iss >> resource;
      std::string rest;
      std::getline(iss, rest);
      std::string value = trim(rest);

      std::unique_lock<std::mutex> lk(table_mtx);
      auto &ls = lock_table[resource];

      if (!ls.locked || ls.owner != client_id) {
        enqueue_response(client_id, "ERROR not_owner");
      } else {
        ls.value = value;
        enqueue_response(client_id, "OK");
      }
    } else if (cmd == "READ") {
      std::string resource;
      iss >> resource;
      std::unique_lock<std::mutex> lk(table_mtx);
      // Allow reading if it exists (even if locked by someone else, typically
      // allowed in simple KV) Or enforce lock ownership. Code below allows read
      // if key exists.
      if (lock_table.find(resource) == lock_table.end()) {
        enqueue_response(client_id, "VALUE ");
      } else {
        enqueue_response(client_id, "VALUE " + lock_table[resource].value);
      }
    } else {
      enqueue_response(client_id, "ERROR unknown_command");
    }

    // Signal Main thread that we are done processing this message
    // (Actual payloads are in the response_queue)
    std::string ack = "ACK";
    pair.send(zmq::buffer(ack), zmq::send_flags::none);
  }
}

// --- Main Thread ---
int main() {
  zmq::context_t ctx(1);
  zmq::socket_t router(ctx, zmq::socket_type::router);
  router.bind("tcp://*:5555");
  std::cerr << "Lock Server started on tcp://*:5555 (Thread Affinity Mode)\n";

  std::unordered_map<std::string, WorkerInfo *> affinity;
  int worker_count = 0;

  while (true) {
    zmq::message_t client_id_msg;
    zmq::message_t empty;
    zmq::message_t payload_msg;

    // 1. Receive from Client (ROUTER)
    if (!router.recv(client_id_msg, zmq::recv_flags::none))
      continue;
    router.recv(empty, zmq::recv_flags::none);
    router.recv(payload_msg, zmq::recv_flags::none);

    std::string client_id(static_cast<char *>(client_id_msg.data()),
                          client_id_msg.size());
    std::string payload(static_cast<char *>(payload_msg.data()),
                        payload_msg.size());

    // 2. Route to Worker
    if (affinity.find(client_id) == affinity.end()) {
      // Create new worker thread for this client
      std::string endpoint =
          "inproc://worker-" + std::to_string(worker_count++);
      auto *info =
          new WorkerInfo{client_id, zmq::socket_t(ctx, zmq::socket_type::pair)};
      info->socket.bind(endpoint);
      affinity[client_id] = info;

      // Launch thread
      std::thread(worker_thread, &ctx, endpoint, client_id).detach();
      std::cerr << "Spawned worker for new client: " << client_id << "\n";
    }

    // Forward payload to worker
    affinity[client_id]->socket.send(zmq::buffer(payload),
                                     zmq::send_flags::none);

    // 3. Wait for Worker ACK
    // This ensures the worker has finished touching the global state/queue
    // before we try to send replies.
    zmq::message_t ack_msg;
    affinity[client_id]->socket.recv(ack_msg, zmq::recv_flags::none);

    // 4. Flush Response Queue
    // Send ANY waiting messages to ANY clients (e.g., RELEASE might trigger a
    // GRANTED for another client)
    std::lock_guard<std::mutex> lk(queue_mtx);
    while (!response_queue.empty()) {
      auto resp = response_queue.front();
      response_queue.pop_front();

      // Router send: [Identity][Empty][Payload]
      router.send(zmq::buffer(resp.first), zmq::send_flags::sndmore);
      router.send(zmq::message_t(), zmq::send_flags::sndmore);
      router.send(zmq::buffer(resp.second), zmq::send_flags::none);
    }
  }

  return 0;
}