#pragma once

#include "ctx/operation.h"
#include "ctx/scheduler.h"

namespace ctx {

template <typename T>
T& maybe_deref(T& x) {
  return x;
}

template <typename T>
T& maybe_deref(T* x) {
  return *x;
}

template <typename T>
bool is_null(T&) {
  return false;
}

template <typename T>
bool is_null(T* x) {
  return x == nullptr;
}

template <typename Data>
operation<Data>::operation(Data data, std::function<void()> fn,
                           scheduler<Data>& sched, op_id id)
    :
#ifdef CTX_ENABLE_ASAN
      fake_stack_(nullptr),
      bottom_old_(nullptr),
      size_old_(0),
#endif
      id_(std::move(id)),
      data_(data),
      sched_(sched),
      fn_(std::move(fn)),
      running_(false),
      reschedule_(false),
      finished_(false) {
}

template <typename Data>
operation<Data>::~operation() {
  sched_.stack_manager_.dealloc(stack_);
}

#ifdef CTX_ENABLE_ASAN
template <typename Data>
void operation<Data>::enter_op_start_switch() {
  __sanitizer_start_switch_fiber(&fake_stack_, stack_.get_allocated_mem(),
                                 kStackSize);
}

template <typename Data>
void operation<Data>::enter_op_finish_switch() {
  __sanitizer_finish_switch_fiber(fake_stack_,
                                  &bottom_old_,  // NOLINT
                                  &size_old_);
}

template <typename Data>
void operation<Data>::exit_op_start_switch() {
  __sanitizer_start_switch_fiber(&fake_stack_, bottom_old_, size_old_);
}

template <typename Data>
void operation<Data>::exit_op_finish_switch() {
  __sanitizer_finish_switch_fiber(fake_stack_, nullptr, nullptr);
}
#else
template <typename Data>
void operation<Data>::enter_op_start_switch() {}

template <typename Data>
void operation<Data>::enter_op_finish_switch() {}

template <typename Data>
void operation<Data>::exit_op_start_switch() {}

template <typename Data>
void operation<Data>::exit_op_finish_switch() {}
#endif

template <typename Data>
void operation<Data>::on_transition(transition t, op_id const& callee) {
  if (!is_null(data_)) {
    maybe_deref(data_).transition(t, id_, callee);
  }
}

template <typename Data>
void operation<Data>::resume() {
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

  on_transition(transition::ACTIVATE);
  this_op = this;
  enter_op_start_switch();
  auto const t = jump_fcontext(op_ctx_, this);
  exit_op_finish_switch();

  op_ctx_ = t.ctx;
  auto const finished = t.data == nullptr;

  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (reschedule_) {
      sched_.enqueue_work(this->shared_from_this());
      reschedule_ = false;
    }
    finished_ = finished;
    running_ = false;
  }
}

template <typename Data>
void operation<Data>::suspend(bool finished) {
  on_transition(finished ? transition::FIN : transition::DEACTIVATE);
  std::shared_ptr<operation<Data>> self =
      finished ? nullptr : this->shared_from_this();
  exit_op_start_switch();
  auto const t =
      jump_fcontext(main_ctx_, finished ? nullptr : reinterpret_cast<void*>(1));
  enter_op_finish_switch();

  main_ctx_ = t.ctx;
}

template <typename Data>
void operation<Data>::start() {
  fn_();
  suspend(true);
}

template <typename Data>
void operation<Data>::init() {
  stack_ = sched_.stack_manager_.alloc();
  op_ctx_ = make_fcontext(stack_.get_stack(), kStackSize, execute<Data>);
}

}  // namespace ctx
