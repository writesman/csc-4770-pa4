#ifndef SESSION_REGISTRY_HPP
#define SESSION_REGISTRY_HPP

#include "lock_manager.hpp"
#include <string>
#include <unordered_map>
#include <zmq.hpp>

void handle_worker_session(zmq::context_t *ctx, std::string session_worker_id,
                           LockManager *manager);

class SessionRegistry {
private:
  std::unordered_map<std::string, std::string> affinity_map;
  int worker_seq = 0;
  zmq::context_t *ctx;
  LockManager *manager;

public:
  SessionRegistry(zmq::context_t *context, LockManager *mgr);
  std::string get_worker_for_client(const std::string &client_id,
                                    zmq::socket_t &backend);
};

#endif