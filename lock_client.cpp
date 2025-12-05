#include "protocol.hpp"
#include <iostream>
#include <string>
#include <unistd.h>
#include <zmq.hpp>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <resource> <mode> [content] [sleep]"
              << std::endl;
    return 1;
  }

  std::string resource = argv[1];
  std::string mode = argv[2];
  std::string content = (argc > 3) ? argv[3] : "";
  int sleep_time = (argc > 4) ? std::stoi(argv[4]) : 0;

  zmq::context_t ctx(1);
  zmq::socket_t sock(ctx, zmq::socket_type::req);

  std::cout << "CONNECTING to lock server..." << std::endl;
  sock.connect("tcp://localhost:5555");

  // 1. Request Lock
  std::string req_str =
      std::string(Protocol::CMD_LOCK) + " " + resource + " " + mode;
  std::cout << "REQUESTING lock for " << resource << "..." << std::endl;

  sock.send(zmq::buffer(req_str), zmq::send_flags::none);

  zmq::message_t reply;
  sock.recv(reply, zmq::recv_flags::none);

  if (reply.to_string() == Protocol::MSG_OK) {
    std::cout << "LOCKED " << resource << "." << std::endl;

    // 2. Critical Section
    if (mode == Protocol::MODE_WRITE) {
      if (sleep_time > 0)
        sleep(sleep_time);
      std::cout << "WRITING to " << resource << ": " << content << std::endl;
    } else {
      std::cout << "READING from " << resource << " (simulated)." << std::endl;
      if (sleep_time > 0)
        sleep(sleep_time);
    }

    // 3. Release Lock
    std::cout << "RELEASING " << resource << "." << std::endl;
    std::string unlock_str =
        std::string(Protocol::CMD_UNLOCK) + " " + resource + " " + mode;
    sock.send(zmq::buffer(unlock_str), zmq::send_flags::none);

    sock.recv(reply, zmq::recv_flags::none);
    std::cout << "UNLOCKED " << resource << "." << std::endl;
  } else {
    std::cerr << "Failed to acquire lock." << std::endl;
  }

  return 0;
}