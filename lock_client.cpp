// lock_client.cpp
// Build: g++ -std=c++17 -O2 -pthread lock_client.cpp -lzmq -o lock_client
// Usage examples:
// ./lock_client file_A WRITE "Hello Distributed World" 5
// ./lock_client file_A READ

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <zmq.hpp>

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "Usage: ./lock_client <resource> <READ|WRITE> [value] "
                 "[sleep_seconds]\n";
    return 1;
  }
  std::string resource = argv[1];
  std::string op = argv[2];

  std::string value;
  int sleep_seconds = 0;
  if (op == "WRITE") {
    if (argc < 4) {
      std::cerr << "WRITE requires a value argument\n";
      return 1;
    }
    value = argv[3];
    if (argc >= 5)
      sleep_seconds = std::stoi(argv[4]);
  }

  zmq::context_t ctx(1);
  zmq::socket_t req(ctx, zmq::socket_type::req);

  // --- TIMEOUT CONFIGURATION (Required for Fault Tolerance) ---
  // If the server doesn't reply within 3000ms, the recv() call will return
  // (or throw, depending on version) instead of blocking forever.
  const int timeout_ms = 3000;
  req.set(zmq::sockopt::rcvtimeo, timeout_ms);
  req.set(zmq::sockopt::sndtimeo, timeout_ms);

  req.connect("tcp://127.0.0.1:5555");
  std::cout << "CONNECTING to lock server at tcp://localhost:5555\n";

  auto send_and_recv = [&](const std::string &msg,
                           std::string &reply_out) -> bool {
    // Send
    try {
      req.send(zmq::buffer(msg), zmq::send_flags::none);
    } catch (const zmq::error_t &e) {
      std::cerr << "Send failed: " << e.what() << "\n";
      return false;
    }

    // Receive with timeout
    zmq::message_t reply;
    try {
      // In cppzmq, recv usually returns an optional or bool.
      // If timeout occurs, it may return false or throw EAGAIN.
      auto res = req.recv(reply, zmq::recv_flags::none);
      if (!res) {
        std::cerr << "Error: Server timed out (no reply within " << timeout_ms
                  << "ms)\n";
        return false;
      }
    } catch (const zmq::error_t &e) {
      std::cerr << "Recv error: " << e.what() << "\n";
      return false;
    }

    reply_out.assign(static_cast<char *>(reply.data()), reply.size());
    return true;
  };

  // 1) Acquire
  std::string out;
  std::string acquire_cmd = "ACQUIRE " + resource;
  std::cout << "REQUESTING lock for resource: " << resource << "\n";

  // Loop until acquired or error?
  // For this assignment, we just fail if the server is dead.
  if (!send_and_recv(acquire_cmd, out))
    return 1;

  // The server replies "GRANTED <resource>" or we block inside recv if queued?
  // Actually, our server implementation is asynchronous for waiters.
  // BUT the ZMQ REQ socket expects a strict Send-Recv pattern.
  // If the server queues us and DOES NOT reply, the 'req.recv' above will hit
  // the timeout. This is a known issue with blocking REQ/ROUTER locks. However,
  // given the timeout requirement, if we timeout here, it usually means we are
  // stuck in the wait queue or the server is dead. We can retry reading (if the
  // socket allows) or just exit.

  std::cout << out << "\n";
  if (out.rfind("GRANTED", 0) != 0) {
    std::cerr << "Failed to acquire lock: " << out << "\n";
    return 1;
  }
  std::cout << "LOCKED " << resource << "\n";

  if (op == "WRITE") {
    if (sleep_seconds > 0) {
      std::cout << "Sleeping for " << sleep_seconds
                << " seconds before WRITE...\n";
      std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
    }
    std::cout << "WRITING value to " << resource << ": " << value << "\n";
    std::string write_cmd = "WRITE " + resource + " " + value;
    if (!send_and_recv(write_cmd, out))
      return 1;
    std::cout << "WRITE reply: " << out << "\n";
  } else if (op == "READ") {
    std::string read_cmd = "READ " + resource;
    if (!send_and_recv(read_cmd, out))
      return 1;
    if (out.rfind("VALUE ", 0) == 0) {
      std::string val = out.substr(6);
      std::cout << "READING value from " << resource << ": " << val << "\n";
    } else {
      std::cout << "READ reply: " << out << "\n";
    }
  }

  // Release
  std::string rel_cmd = "RELEASE " + resource;
  std::cout << "RELEASING lock for resource: " << resource << "\n";
  if (!send_and_recv(rel_cmd, out))
    return 1;
  std::cout << out << "\n";
  std::cout << "UNLOCKED " << resource << "\n";

  return 0;
}