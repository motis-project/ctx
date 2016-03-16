#pragma once

#include <memory>
#include <mutex>

#include "ctx/operation.h"

namespace ctx {

template <typename Data>
struct condition_variable {
  condition_variable();

  template <typename Predicate>
  void wait(std::unique_lock<std::mutex>& lock, Predicate pred);

  void wait();
  void notify();

  std::weak_ptr<operation<Data>> caller_;
};

}  // namespace ctx
