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
    assert(!finished_);

    running_mutex_.lock();
    if (running_) {
      reschedule_ = true;
      running_mutex_.unlock();
      return;
    }

    running_ = true;

    if (stack_ == nullptr) {
      init();
    }

    this_op = this;
    boost::context::jump_fcontext(&main_ctx_, op_ctx_, me(), false);

    if (reschedule_) {
      sched_.enqueue(this);
      reschedule_ = false;
    }
    running_mutex_.unlock();

    if (finished_) {
      return;  // post return value
    }
  }

  void suspend() {
    running_mutex_.lock();
    running_ = false;
    boost::context::jump_fcontext(&op_ctx_, main_ctx_, 0, false);
    running_mutex_.unlock();
  }

  static thread_local operation* this_op;

  void start() {
    running_mutex_.unlock();
    fn_();
    finished_ = true;
    suspend();
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

  std::mutex running_mutex_;
  bool running_;
  bool reschedule_;
  bool finished_;
};

inline void execute(intptr_t op_ptr) {
  reinterpret_cast<operation*>(op_ptr)->start();
}

}  // namespace ctx