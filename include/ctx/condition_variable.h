#pragma once

#include "ctx/condition_state.h"

namespace ctx {

struct condition_variable {
  condition_variable() : op_(op::this_op) {}

  template <typename Predicate>
  void wait(Predicate pred) {
    state_.mutex.lock();
    while (!pred()) {
      wait();
    }
  }

  void wait() {
    state_.suspended = false;
    op_->suspend();
  }

  void notify() {
    state_.mutex.lock();
    op_->scheduler.resume(op_);
    state_.mutex.unlock();
  }

  operation* op_;
  condition_state state_;
};

}  // namespace ctx