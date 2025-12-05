#ifndef SESSION_REGISTRY_HPP
#define SESSION_REGISTRY_HPP

#include "lock_manager.hpp"
#include <string>
#include <unordered_map>
#include <zmq.hpp>

// Function that runs on a detached worker thread, managing a client's request
// session.
void handle_worker_session(zmq::context_t *ctx, std::string session_worker_id,
                           LockManager *manager);

// SessionRegistry manages the affinity mapping (Client ID -> Worker Thread ID)
// and thread lifecycle.
class SessionRegistry {
private:
  std::unordered_map<std::string, std::string>
      affinity_map; // Maps ClientID to WorkerID
  int worker_seq = 0;
  zmq::context_t *ctx;
  LockManager *manager;

public:
  SessionRegistry(zmq::context_t *context, LockManager *mgr);

  // Retrieves the worker ID for a client, spawning a new thread and waiting for
  // its handshake if necessary.
  std::string get_worker_for_client(const std::string &client_id,
                                    zmq::socket_t &backend);
};

#endif