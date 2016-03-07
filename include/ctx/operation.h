#pragma once

#include <cinttypes>
#include <functional>
#include <mutex>
#include <memory>
#include <atomic>

#include "boost/context/all.hpp"

#include "ctx/op_id.h"
#include "ctx/scheduler.h"

#define CTX_STRING1(x) #x
#define CTX_STRING2(x) CTX_STRING1(x)
#define CTX_LOCATION __FILE__ ":" CTX_STRING2(__LINE__)

#define ctx_call(fn) ctx::call(fn, ctx::op_id("unknown", CTX_LOCATION))

namespace ctx {

void execute(intptr_t);

struct operation : public std::enable_shared_from_this<operation> {
  operation(std::function<void()> fn, scheduler& sched, op_id id)
      : id_(std::move(id)),
        sched_(sched),
        fn_(std::move(fn)),
        running_(false),
        reschedule_(false),
        finished_(false) {
    sched.tracker_.print_status();
  }

  ~operation() { sched_.stack_manager_.dealloc(stack_); }

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

    if (stack_.get_stack() == nullptr) {
      init();
    }

    this_op = this;
    sched_.on_resume(id_);
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
    finished ? sched_.on_finish(id_) : sched_.on_suspend(id_, id_);
    std::shared_ptr<operation> self = finished ? nullptr : shared_from_this();
    boost::context::jump_fcontext(&op_ctx_, main_ctx_, finished, false);
  }

  void start() {
    fn_();
    suspend(true);
  }

  void init() {
    stack_ = sched_.stack_manager_.alloc();
    op_ctx_ =
        boost::context::make_fcontext(stack_.get_stack(), kStackSize, execute);
  }

  intptr_t me() const { return reinterpret_cast<intptr_t>(this); }

  op_id id_;

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

template <typename Fn>
auto call(Fn fn, op_id id) -> std::shared_ptr<future<decltype(fn())>> {
  return operation::this_op->sched_.post(std::forward<Fn>(fn), std::move(id));
}

inline void execute(intptr_t op_ptr) {
  reinterpret_cast<operation*>(op_ptr)->start();
}

}  // namespace ctx
