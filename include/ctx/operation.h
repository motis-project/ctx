#pragma once

#include <cinttypes>
#include <functional>
#include <memory>
#include <atomic>

#include "boost/context/all.hpp"

#include "ctx/scheduler.h"

namespace ctx {

void execute(intptr_t);

struct operation : public std::enable_shared_from_this<operation> {
  operation(std::function<void()> fn, scheduler& sched)
      : stack_({nullptr, 0}),
        sched_(sched),
        fn_(fn),
        running_(false),
        reschedule_(false),
        finished_(false) {}

  ~operation() {
    sched_.stack_manager_.dealloc(stack_);
    stack_.mem = nullptr;
  }

  template <typename Fn>
  auto call(Fn fn) -> std::shared_ptr<future<decltype(fn())>> {
    return sched_.post(std::forward<Fn>(fn));
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

    if (stack_.mem == nullptr) {
      init();
    }

    this_op = this;
    auto finished =
        boost::context::jump_fcontext(&main_ctx_, op_ctx_, me(), false);

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (reschedule_) {
        sched_.enqueue(shared_from_this());
        reschedule_ = false;
      }
      finished_ = finished;
      running_ = false;
    }
  }

  void suspend(bool finished) {
    std::shared_ptr<operation> self;
    if (!finished) {
      self = shared_from_this();
    }
    boost::context::jump_fcontext(&op_ctx_, main_ctx_, finished, false);
  }

  void start() {
    fn_();
    suspend(true);
  }

  void init() {
    stack_ = sched_.stack_manager_.alloc();
    op_ctx_ = boost::context::make_fcontext(stack_.mem, kStackSize, execute);
  }

  intptr_t me() const { return reinterpret_cast<intptr_t>(this); }

  stack_handle stack_;
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