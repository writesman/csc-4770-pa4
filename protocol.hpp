// protocol.hpp
#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string_view>

namespace Protocol {
// Commands
constexpr std::string_view CMD_LOCK = "LOCK";
constexpr std::string_view CMD_UNLOCK = "UNLOCK";

// Lock Modes
constexpr std::string_view MODE_READ = "READ";
constexpr std::string_view MODE_WRITE = "WRITE";

// Server Responses
constexpr std::string_view MSG_OK = "OK";
constexpr std::string_view MSG_ERROR = "ERROR";

// Internal Worker Signal (Worker -> Backend)
constexpr std::string_view MSG_READY = "READY";
} // namespace Protocol

#endif // PROTOCOL_HPP