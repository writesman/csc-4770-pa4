// lock_client.cpp
#include <cstdio>
#include <print>
#include <string>
#include <unistd.h> // for sleep
#include <zmq.hpp>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::println(stderr,
                 "Usage: {} <resource_name> <mode> [message] [sleep_time]",
                 argv[0]);
    return 1;
  }

  std::string resource = argv[1];
  std::string mode = argv[2]; // READ or WRITE
  std::string content = (argc > 3) ? argv[3] : "";
  int sleep_time = (argc > 4) ? std::stoi(argv[4]) : 0;

  zmq::context_t ctx(1);
  zmq::socket_t sock(ctx, zmq::socket_type::req);

  std::println("CONNECTING to lock server at tcp://localhost:5555...");
  sock.connect("tcp://localhost:5555");

  // 1. Request Lock
  std::string req_str = "LOCK " + resource + " " + mode;
  std::println("REQUESTING lock for resource: {} ({}).", resource, mode);

  sock.send(zmq::buffer(req_str), zmq::send_flags::none);

  // This RECV blocks until the Server (Worker Thread) says "OK"
  zmq::message_t reply;
  sock.recv(reply, zmq::recv_flags::none);
  std::string reply_str(static_cast<char *>(reply.data()), reply.size());

  if (reply_str == "OK") {
    std::println("LOCKED {}.", resource);

    // 2. Critical Section
    if (mode == "WRITE") {
      if (sleep_time > 0) {
        std::println("Sleeping for {} seconds before WRITE...", sleep_time);
        sleep(sleep_time);
      }
      std::println("WRITING value to {}: {}.", resource, content);
    } else {
      std::println("READING value from {}: (simulated).", resource);
      if (sleep_time > 0)
        sleep(sleep_time);
    }

    // 3. Release Lock
    std::println("RELEASING lock for resource: {}.", resource);
    std::string unlock_str = "UNLOCK " + resource + " " + mode;
    sock.send(zmq::buffer(unlock_str), zmq::send_flags::none);

    sock.recv(reply, zmq::recv_flags::none); // Wait for ACK
    std::println("UNLOCKED {}.", resource);
  } else {
    std::println(stderr, "Failed to acquire lock,");
  }

  return 0;
}