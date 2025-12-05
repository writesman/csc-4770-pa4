// lock_client.cpp
#include <iostream>
#include <string>
#include <unistd.h> // for sleep
#include <zmq.hpp>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <resource_name> <mode> [message] [sleep_time]" << std::endl;
    return 1;
  }

  std::string resource = argv[1];
  std::string mode = argv[2]; // READ or WRITE
  std::string content = (argc > 3) ? argv[3] : "";
  int sleep_time = (argc > 4) ? std::stoi(argv[4]) : 0;

  zmq::context_t ctx(1);
  zmq::socket_t sock(ctx, zmq::socket_type::req);

  std::cout << "CONNECTING to lock server at tcp://localhost:5555..."
            << std::endl;
  sock.connect("tcp://localhost:5555");

  // 1. Request Lock
  std::string req_str = "LOCK " + resource + " " + mode;
  std::cout << "REQUESTING lock for resource: " << resource << " (" << mode
            << ")" << std::endl;

  sock.send(zmq::buffer(req_str), zmq::send_flags::none);

  // This RECV blocks until the Server (Worker Thread) says "OK"
  zmq::message_t reply;
  sock.recv(reply, zmq::recv_flags::none);
  std::string reply_str(static_cast<char *>(reply.data()), reply.size());

  if (reply_str == "OK") {
    std::cout << "LOCKED " << resource << std::endl;

    // 2. Critical Section
    if (mode == "WRITE") {
      if (sleep_time > 0) {
        std::cout << "Sleeping for " << sleep_time << " seconds before WRITE..."
                  << std::endl;
        sleep(sleep_time);
      }
      std::cout << "WRITING value to " << resource << ": " << content
                << std::endl;
    } else {
      std::cout << "READING value from " << resource << " (simulated)"
                << std::endl;
      if (sleep_time > 0)
        sleep(sleep_time);
    }

    // 3. Release Lock
    std::cout << "RELEASING lock for resource: " << resource << std::endl;
    std::string unlock_str = "UNLOCK " + resource + " " + mode;
    sock.send(zmq::buffer(unlock_str), zmq::send_flags::none);

    sock.recv(reply, zmq::recv_flags::none); // Wait for ACK
    std::cout << "UNLOCKED " << resource << std::endl;
  } else {
    std::cerr << "Failed to acquire lock." << std::endl;
  }

  return 0;
}