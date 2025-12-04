// lock_server.cpp
// Build: g++ -std=c++17 -O2 -pthread lock_server.cpp -lzmq -o lock_server

#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <zmq.hpp>

struct LockState {
  bool locked = false;
  std::string owner; // identity string of the client holding the lock
  std::deque<std::string> waiters; // identities waiting (FIFO)
  // optional stored value for the resource
  std::string value;
};

std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(' ');
  if (a == std::string::npos)
    return "";
  size_t b = s.find_last_not_of(' ');
  return s.substr(a, b - a + 1);
}

void send_reply(zmq::socket_t &router, const std::string &client_id,
                const std::string &payload) {
  // send: [identity][empty][payload]
  zmq::message_t id_msg(client_id.data(), client_id.size());
  router.send(id_msg, zmq::send_flags::sndmore);
  router.send(zmq::message_t(), zmq::send_flags::sndmore);
  router.send(zmq::buffer(payload), zmq::send_flags::none);
}

int main() {
  zmq::context_t ctx(1);
  zmq::socket_t router(ctx, zmq::socket_type::router);
  router.bind("tcp://*:5555");
  std::cerr << "Lock Server started on tcp://*:5555\n";

  std::unordered_map<std::string, LockState> lock_table;
  std::mutex table_mtx;

  while (true) {
    zmq::message_t id_msg;
    zmq::message_t empty;
    zmq::message_t payload_msg;

    // Receive identity, empty frame, payload (REQ clients follow this)
    router.recv(id_msg, zmq::recv_flags::none);
    router.recv(empty, zmq::recv_flags::none);
    router.recv(payload_msg, zmq::recv_flags::none);

    std::string client_id(static_cast<char *>(id_msg.data()), id_msg.size());
    std::string payload(static_cast<char *>(payload_msg.data()),
                        payload_msg.size());

    // parse command
    std::istringstream iss(payload);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty()) {
      send_reply(router, client_id, "ERROR empty_command");
      continue;
    }

    if (cmd == "ACQUIRE") {
      std::string resource;
      iss >> resource;
      if (resource.empty()) {
        send_reply(router, client_id, "ERROR missing_resource");
        continue;
      }

      std::unique_lock<std::mutex> lk(table_mtx);
      auto &ls = lock_table[resource];
      if (!ls.locked) {
        // grant immediately
        ls.locked = true;
        ls.owner = client_id;
        lk.unlock();
        send_reply(router, client_id, std::string("GRANTED ") + resource);
      } else {
        // enqueue and do not reply now; the client's REQ will remain blocked
        // until we send a reply later
        ls.waiters.push_back(client_id);
        // no immediate reply
        std::cerr << "Queued client [" << client_id << "] for resource "
                  << resource << "\n";
      }
    } else if (cmd == "RELEASE") {
      std::string resource;
      iss >> resource;
      if (resource.empty()) {
        send_reply(router, client_id, "ERROR missing_resource");
        continue;
      }

      std::unique_lock<std::mutex> lk(table_mtx);
      auto it = lock_table.find(resource);
      if (it == lock_table.end() || !it->second.locked) {
        // nothing to do; per notes send OK or BAD_REQUEST; we send OK
        lk.unlock();
        send_reply(router, client_id, std::string("OK ") + resource);
        continue;
      }

      LockState &ls = it->second;
      if (ls.owner == client_id) {
        // releasing owner
        if (!ls.waiters.empty()) {
          // grant to next waiter (FIFO)
          std::string next = ls.waiters.front();
          ls.waiters.pop_front();
          ls.owner = next;
          // owner remains locked (handed to next)
          // We'll reply to the releasing client with OK, and separately send
          // GRANTED to the waiting client.
          lk.unlock();
          send_reply(router, client_id, std::string("OK ") + resource);
          // send GRANTED to next waiter
          send_reply(router, next, std::string("GRANTED ") + resource);
        } else {
          // no waiters -> unlock
          ls.locked = false;
          ls.owner.clear();
          lk.unlock();
          send_reply(router, client_id, std::string("OK ") + resource);
        }
      } else {
        // client trying to release a lock it doesn't own: per spec, reply OK
        // without changing state
        lk.unlock();
        send_reply(router, client_id, std::string("OK ") + resource);
      }
    } else if (cmd == "WRITE") {
      std::string resource;
      iss >> resource;
      if (resource.empty()) {
        send_reply(router, client_id, "ERROR missing_resource");
        continue;
      }
      // remaining text is the value (allow spaces)
      std::string rest;
      std::getline(iss, rest);
      std::string value = trim(rest);

      std::unique_lock<std::mutex> lk(table_mtx);
      auto &ls = lock_table[resource];
      if (!ls.locked || ls.owner != client_id) {
        lk.unlock();
        send_reply(router, client_id, "ERROR not_owner");
      } else {
        ls.value = value;
        lk.unlock();
        send_reply(router, client_id, "OK");
      }
    } else if (cmd == "READ") {
      std::string resource;
      iss >> resource;
      if (resource.empty()) {
        send_reply(router, client_id, "ERROR missing_resource");
        continue;
      }
      std::unique_lock<std::mutex> lk(table_mtx);
      auto it = lock_table.find(resource);
      if (it == lock_table.end()) {
        lk.unlock();
        send_reply(router, client_id, "VALUE "); // empty
      } else {
        LockState &ls = it->second;
        if (!ls.locked || ls.owner != client_id) {
          // client should hold lock to read (our policy); deny
          std::string v = ls.value;
          lk.unlock();
          send_reply(router, client_id, std::string("VALUE ") + v);
        } else {
          std::string v = ls.value;
          lk.unlock();
          send_reply(router, client_id, std::string("VALUE ") + v);
        }
      }
    } else {
      send_reply(router, client_id,
                 std::string("ERROR unknown_command: ") + cmd);
    }
  }

  return 0;
}
