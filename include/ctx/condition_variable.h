#pragma once

#include <memory>

#include "ctx/operation.h"

namespace ctx {

template <typename Data>
struct operation;

template <typename Data>
struct condition_variable {
  condition_variable();

  template <typename Predicate>
  void wait(Predicate pred);

  void wait();
  void notify();

  std::weak_ptr<operation<Data>> caller_;
};

}  // namespace ctx
