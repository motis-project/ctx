#pragma once

#include <mutex>

namespace ctx {

struct operation;

struct condition_variable {
  condition_variable();

  template <typename Predicate>
  void wait(std::unique_lock<std::mutex>& lock, Predicate pred) {
    while (!pred()) {
      lock.unlock();
      wait();
      lock.lock();
    }
  }

  void wait();
  void notify();

  std::weak_ptr<operation> caller_;
};

}  // namespace ctx