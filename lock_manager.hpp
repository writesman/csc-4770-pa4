#ifndef LOCK_MANAGER_HPP
#define LOCK_MANAGER_HPP

#include "protocol.hpp"
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class LockManager {
private:
  std::unordered_map<std::string, std::unique_ptr<std::shared_mutex>> locks;
  std::mutex global_map_mutex;

  std::shared_mutex *get_lock(const std::string &resource) {
    std::lock_guard<std::mutex> guard(global_map_mutex);
    if (locks.find(resource) == locks.end()) {
      locks[resource] = std::make_unique<std::shared_mutex>();
    }
    return locks[resource].get();
  }

public:
  void acquire(const std::string &resource, const std::string &mode) {
    std::shared_mutex *mtx = get_lock(resource);

    if (mode == Protocol::MODE_WRITE) {
      mtx->lock();
    } else {
      mtx->lock_shared();
    }
  }

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