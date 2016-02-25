#pragma once

#include <cinttypes>
#include <functional>
#include <atomic>

#include "boost/context/all.hpp"

#include "ctx/scheduler.h"
#include "ctx/condition_state.h"

namespace ctx {

struct operation;
void execute(intptr_t);

struct operation {
  friend void execute(intptr_t);

  operation(std::function<void()> fn, scheduler& sched)
      : stack_(nullptr),
        sched_(sched),
        fn_(fn),
        running_ ATOMIC_FLAG_INIT,
        finished_(false) {}

  ~operation() { sched_.stack_manager_.dealloc(stack_); }

  void resume() {
    assert(!finished_);
    if (running_.test_and_set()) {
      return;
    }

    if (stack_ == nullptr) {
      init();
    }

    this_op = this;
    auto ret = boost::context::jump_fcontext(&main_ctx_, op_ctx_, me(), false);
    running_.clear();
    if (finished_) {
      return;
    }

    auto& cond_state = *reinterpret_cast<condition_state*>(ret);
    cond_state.suspended = true;
    cond_state.mutex.unlock();
  }

  void suspend() {
    boost::context::jump_fcontext(&op_ctx_, main_ctx_, 0, false);
  }

private:
  void init() {
    stack_ = sched_.stack_manager_.alloc();
    op_ctx_ = boost::context::make_fcontext(stack_, kStackSize, execute);
  }

  intptr_t me() const { return reinterpret_cast<intptr_t>(this); }

  void* stack_;
  boost::context::fcontext_t op_ctx_;
  boost::context::fcontext_t main_ctx_;
  scheduler& sched_;
  std::function<void()> fn_;
  std::atomic_flag running_;
  bool finished_;
  static thread_local operation* this_op;
};

inline void execute(intptr_t op_ptr) {
  auto& op = *reinterpret_cast<operation*>(op_ptr);
  op.fn_();
  op.finished_ = true;
  boost::context::jump_fcontext(&op.op_ctx_, op.main_ctx_, op_ptr, false);
}

}  // namespace ctx