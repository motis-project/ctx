#pragma once

#include <cinttypes>
#include <functional>
#include <memory>
#include <mutex>

#include "boost/context/fcontext.hpp"

#include "ctx/future.h"
#include "ctx/op_id.h"
#include "ctx/stack_manager.h"
#include "ctx/thread_local.h"
#include "ctx_config.h"

#ifdef CTX_ENABLE_ASAN
extern "C" {
void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom,
                                    size_t size);
void __sanitizer_finish_switch_fiber(void* fake_stack_save,
                                     const void** bottom_old, size_t* size_old);
}
#endif

namespace ctx {

template <typename Data>
struct scheduler;

enum class transition { RESUME, SUSPEND, ENQUEUE, ACTIVATE, DEACTIVATE, FIN };

template <typename Data>
struct operation : public std::enable_shared_from_this<operation<Data>> {
  operation(Data, std::function<void()>, scheduler<Data>&, op_id);
  ~operation();

  void enter_op_start_switch();
  void enter_op_finish_switch();
  void exit_op_start_switch();
  void exit_op_finish_switch();

  void on_transition(transition t, op_id const& id = op_id());
  void resume();
  void suspend(bool finished);
  void start();
  void init();
  intptr_t me() const;

#ifdef CTX_ENABLE_ASAN
  void* fake_stack_;
  void const* bottom_old_;
  size_t size_old_;
#endif

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

extern CTX_ATTRIBUTE_TLS void* this_op;

template <typename Data>
inline void execute(intptr_t op_ptr) {
  reinterpret_cast<operation<Data>*>(op_ptr)->enter_op_finish_switch();
  reinterpret_cast<operation<Data>*>(op_ptr)->start();
}

template <typename Data>
operation<Data>& current_op() {
  return *reinterpret_cast<operation<Data>*>(this_op);
}

}  // namespace ctx
