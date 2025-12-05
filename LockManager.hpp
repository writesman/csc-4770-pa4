// LockManager.hpp
#ifndef LOCK_MANAGER_HPP
#define LOCK_MANAGER_HPP

#include "protocol.hpp"
#include <condition_variable>
#include <mutex>
#include <print>
#include <string>
#include <unordered_map>

enum LockMode { UNLOCKED, READ, WRITE };

struct ResourceState {
  LockMode mode = UNLOCKED;
  int active_readers = 0;
  std::condition_variable cv;
};

class LockManager {
private:
  std::unordered_map<std::string, ResourceState> resources;
  std::mutex global_mutex;

public:
  void acquire(const std::string &resource, const std::string &mode) {
    std::unique_lock<std::mutex> lock(global_mutex);
    ResourceState &state = resources[resource];

    // Simple string comparison using constants
    if (mode == Protocol::MODE_WRITE) {
      while (state.mode != UNLOCKED) {
        state.cv.wait(lock);
      }
      state.mode = WRITE;
    } else if (mode == Protocol::MODE_READ) {
      while (state.mode == WRITE) {
        state.cv.wait(lock);
      }
      state.mode = READ;
      state.active_readers++;
    }
    std::println("[LOG] Locked {} in {} mode.", resource, mode);
  }

  void release(const std::string &resource) {
    std::unique_lock<std::mutex> lock(global_mutex);
    if (!resources.contains(resource))
      return;

    ResourceState &state = resources[resource];
    if (state.mode == WRITE) {
      state.mode = UNLOCKED;
    } else if (state.mode == READ) {
      state.active_readers--;
      if (state.active_readers == 0)
        state.mode = UNLOCKED;
    }

    std::println("[LOG] Released {}.", resource);
    state.cv.notify_all();
  }
};

#endif