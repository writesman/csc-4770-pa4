#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string_view>

namespace Protocol {
constexpr std::string_view CMD_LOCK = "LOCK";
constexpr std::string_view CMD_UNLOCK = "UNLOCK";
constexpr std::string_view MODE_READ = "READ";
constexpr std::string_view MODE_WRITE = "WRITE";
constexpr std::string_view MSG_OK = "OK";
constexpr std::string_view MSG_ERROR = "ERROR";
constexpr std::string_view MSG_READY = "READY";
} // namespace Protocol

#endif