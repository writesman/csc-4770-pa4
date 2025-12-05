#include "LockClient.hpp"
#include "protocol.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

// Helper to handle File I/O errors cleanly
void write_file(const std::string &filename, const std::string &data) {
  std::ofstream outfile(filename, std::ios::trunc);
  if (!outfile) {
    std::cerr << "[CLIENT] Error: Could not write to file '" << filename
              << "'\n";
    return;
  }
  outfile << data;
}

std::string read_file(const std::string &filename) {
  std::ifstream infile(filename);
  if (!infile) {
    std::cerr << "[CLIENT] Error: Could not open file '" << filename << "'\n";
    return "";
  }
  return std::string((std::istreambuf_iterator<char>(infile)),
                     std::istreambuf_iterator<char>());
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <resource> <mode> [content] [sleep_time]\n";
    return 1;
  }

  std::string resource = argv[1];
  std::string mode = argv[2];
  std::string content = (argc > 3) ? argv[3] : "";
  int sleep_sec = (argc > 4) ? std::stoi(argv[4]) : 0;

  LockClient client;

  std::cout << "[CLIENT] Requesting lock for " << resource << "...\n";

  if (client.send_request(Protocol::CMD_LOCK, resource, mode)) {
    std::cout << "[CLIENT] Acquired Lock!\n";

    // --- CRITICAL SECTION ---
    if (mode == Protocol::MODE_WRITE) {
      if (sleep_sec) {
        std::cout << "[CLIENT] Simulating work (" << sleep_sec << "s)...\n";
        std::this_thread::sleep_for(std::chrono::seconds(sleep_sec));
      }

      // Perform actual file write
      write_file(resource, content);
      std::cout << "[CLIENT] WRITING value to " << resource << ": " << content
                << "\n";

    } else {
      // Perform actual file read
      std::string data = read_file(resource);

      std::cout << "[CLIENT] READING value from " << resource << ": " << data
                << "\n";

      if (sleep_sec)
        std::this_thread::sleep_for(std::chrono::seconds(sleep_sec));
    }
    // ------------------------

    client.send_request(Protocol::CMD_UNLOCK, resource, mode);
    std::cout << "[CLIENT] Released Lock.\n";
  } else {
    std::cerr << "[CLIENT] Error: Lock request denied.\n";
    return 1;
  }

  return 0;
}