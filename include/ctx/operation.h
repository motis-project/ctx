#pragma once

#include <cinttypes>
#include <functional>
#include <atomic>

#include "boost/context/all.hpp"

#include "ctx/scheduler.h"

namespace ctx {

struct condition_variable;
struct operation;
void execute(intptr_t);

struct operation {
  friend condition_variable;
  friend void execute(intptr_t);

  operation(std::function<void()> fn, scheduler& sched)
      : stack_(nullptr),
        sched_(sched),
        fn_(fn),
        running_(false),
        reschedule_(false),
        finished_(false) {}

  ~operation() {
    sched_.stack_manager_.dealloc(stack_);
    stack_ = nullptr;
  }

  void resume() {
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (finished_) {
        return;
      }
      if (running_) {
        reschedule_ = true;
        return;
      }
      running_ = true;
    }

    if (stack_ == nullptr) {
      init();
    }

    this_op = this;
    auto finished =
        boost::context::jump_fcontext(&main_ctx_, op_ctx_, me(), false);

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (reschedule_) {
        sched_.enqueue(this);
        reschedule_ = false;
      }
      finished_ = finished;
      running_ = false;
    }
  }

  void suspend(bool finished) {
    boost::context::jump_fcontext(&op_ctx_, main_ctx_, finished, false);
  }

  void start() {
    fn_();
    suspend(true);
  }

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

  std::mutex state_mutex_;
  bool running_;
  bool reschedule_;
  bool finished_;

  static thread_local operation* this_op;
};

inline void execute(intptr_t op_ptr) {
  reinterpret_cast<operation*>(op_ptr)->start();
}

}  // namespace ctx