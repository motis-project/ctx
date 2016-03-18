#pragma once

#include <cinttypes>
#include <functional>
#include <mutex>
#include <memory>

#include "boost/context/all.hpp"

#include "ctx/stack_manager.h"
#include "ctx/future.h"
#include "ctx/op_id.h"

namespace ctx {

template <typename Data>
struct scheduler;

enum class client_state { RUN, WAIT, FIN };
enum class ctx_state { ENQUEUED, ACTIVE, INACTIVE, FIN };

template <typename Data>
struct operation : public std::enable_shared_from_this<operation<Data>> {
  operation(Data, std::function<void()>, scheduler<Data>&, op_id);
  ~operation();

  void resume();
  void suspend(bool finished);
  void start();
  void init();
  intptr_t me() const;

  Data data_;
  op_id id_;

  stack_handle stack_;
  boost::context::fcontext_t op_ctx_;
  boost::context::fcontext_t main_ctx_;

  scheduler<Data>& sched_;
  std::function<void()> fn_;

  std::mutex state_mutex_;
  bool running_;
  bool reschedule_;
  bool finished_;
};

template <typename Data>
inline void execute(intptr_t op_ptr) {
  reinterpret_cast<operation<Data>*>(op_ptr)->start();
}

static thread_local void* this_op;

template <typename T>
T& maybe_deref(T& x) {
  return x;
}

template <typename T>
T& maybe_deref(T* x) {
  return *x;
}

template <typename Data>
void on_this_op_suspend(op_id const& callee) {
  auto& current_op = *reinterpret_cast<operation<Data>*>(this_op);
  maybe_deref(current_op.data_).on_suspend(current_op.id_, callee);
}

template <typename Data>
void on_this_op_resume() {
  auto& current_op = *reinterpret_cast<operation<Data>*>(this_op);
  maybe_deref(current_op.data_).on_resume(current_op.id_);
}

}  // namespace ctx
