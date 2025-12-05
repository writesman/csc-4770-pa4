#include "lock_client.hpp"
#include "file_io.hpp"
#include "protocol.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// --- MAIN CONTROLLER ---
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

  // 1. Connection Output
  std::cout << "CONNECTING to lock server at tcp://localhost:5555\n";
  LockClient client;

  // 2. Creating Lock Output (Matches WRITE example)
  if (mode == Protocol::MODE_WRITE) {
    std::cout << "CREATING lock for resource: " << resource << "\n";
  }

  // 3. Requesting Lock Output
  std::cout << "REQUESTING lock for resource: " << resource << "\n";

  if (client.send_request(std::string(Protocol::CMD_LOCK), resource, mode)) {
    // 4. Locked Output
    std::cout << "LOCKED " << resource << "\n";

    // --- CRITICAL SECTION EXECUTION ---
    if (mode == Protocol::MODE_WRITE) {
      // Write Output
      if (sleep_sec) {
        std::cout << "Sleeping for " << sleep_sec
                  << " seconds before WRITE...\n";
        std::this_thread::sleep_for(std::chrono::seconds(sleep_sec));
      }
      write_to_disk(resource, content);
      std::cout << "WRITING value to " << resource << ": " << content << "\n";

    } else {
      // Read Output
      std::string data = read_from_disk(resource);
      std::cout << "READING value from " << resource << ": " << data << "\n";
    }
    // --- END CRITICAL SECTION ---

    // 5. Release Output
    std::cout << "RELEASING lock for resource: " << resource << "\n";
    client.send_request(std::string(Protocol::CMD_UNLOCK), resource, mode);

    // 6. Deleting Output (Matches READ example)
    if (mode == Protocol::MODE_READ)
      std::cout << "DELETING lock for resource: " << resource << "\n";

    // 7. Unlocked Output
    std::cout << "UNLOCKED " << resource << "\n";
  } else {
    std::cerr << "Failed to acquire lock.\n";
    return 1;
  }

  return 0;
}