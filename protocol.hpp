#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <sstream>
#include <string>
#include <string_view>

namespace Protocol {
// Client/Server command and mode constants.
constexpr std::string_view CMD_LOCK = "LOCK";
constexpr std::string_view CMD_UNLOCK = "UNLOCK";
constexpr std::string_view MODE_READ = "READ";
constexpr std::string_view MODE_WRITE = "WRITE";
constexpr std::string_view MSG_OK = "OK";
constexpr std::string_view MSG_ERROR = "ERROR";

constexpr std::string_view MSG_READY =
    "READY"; // Signal from worker to main router.

struct LockRequest {
  std::string command;
  std::string resource;
  std::string mode;

  std::string to_string() const {
    return command + " " + resource + " " + mode;
  }

  static LockRequest parse(const std::string &payload) {
    std::stringstream ss(payload);
    LockRequest req;
    ss >> req.command >> req.resource >> req.mode;
    return req;
  }
};
} // namespace Protocol

#endif