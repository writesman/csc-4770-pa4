#ifndef LOCK_MANAGER_HPP
#define LOCK_MANAGER_HPP

#include "protocol.hpp"
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

// LockManager provides thread-safe concurrency control for named resources.
// It implements the core Read/Write lock logic.
class LockManager {
private:
  // Map holds synchronization objects (mutexes) for each resource.
  std::unordered_map<std::string, std::unique_ptr<std::shared_mutex>> locks;
  // Mutex to protect access to the 'locks' map during creation/lookup.
  std::mutex global_map_mutex;

  // Retrieves or lazily creates the mutex for a given resource name.
  // This supports the dynamic naming requirement and enables fine-grained
  // locking.
  std::shared_mutex *get_lock(const std::string &resource) {
    std::lock_guard<std::mutex> guard(global_map_mutex);
    if (locks.find(resource) == locks.end()) {
      // Use std::unique_ptr for reliable memory management.
      locks[resource] = std::make_unique<std::shared_mutex>();
    }
    return locks[resource].get();
  }

public:
  // Acquires a lock, blocking the calling worker thread until permission is
  // granted.
  void acquire(const std::string &resource, const std::string &mode) {
    std::shared_mutex *mtx = get_lock(resource);

    if (mode == Protocol::MODE_WRITE) {
      // Exclusive lock: blocks all other readers and writers.
      mtx->lock();
    } else {
      // Shared lock: allows multiple concurrent readers, blocks writers.
      mtx->lock_shared();
    }
  }

  // Releases the lock held by the current worker thread.
  void release(const std::string &resource, const std::string &mode) {
    std::shared_mutex *mtx = get_lock(resource);

    if (mode == Protocol::MODE_WRITE) {
      mtx->unlock();
    } else {
      mtx->unlock_shared();
    }
  }
};

#endif