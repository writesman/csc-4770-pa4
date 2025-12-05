#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <sstream>
#include <string>
#include <string_view>

namespace Protocol {
// --- Message Constants for Client/Server Wire Protocol ---
constexpr std::string_view CMD_LOCK = "LOCK";
constexpr std::string_view CMD_UNLOCK = "UNLOCK";
constexpr std::string_view MODE_READ = "READ";
constexpr std::string_view MODE_WRITE = "WRITE";
constexpr std::string_view MSG_OK = "OK";
constexpr std::string_view MSG_ERROR = "ERROR";

// Internal signal used by worker threads to notify the main router they are
// online.
constexpr std::string_view MSG_READY = "READY";

// Standardized structure for communication payloads.
struct LockRequest {
  std::string command;
  std::string resource;
  std::string mode;

  // Serializes request into the expected "CMD RESOURCE MODE" string format.
  std::string to_string() const {
    return command + " " + resource + " " + mode;
  }

  // Deserializes a raw payload string into the request struct.
  static LockRequest parse(const std::string &payload) {
    std::stringstream ss(payload);
    LockRequest req;
    ss >> req.command >> req.resource >> req.mode;
    return req;
  }
};
} // namespace Protocol

#endif