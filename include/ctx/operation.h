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
struct operation;

template <typename Data>
inline void execute(intptr_t op_ptr) {
  reinterpret_cast<operation<Data>*>(op_ptr)->start();
}

template <typename Data>
struct scheduler;

template <typename Data>
struct operation : public std::enable_shared_from_this<operation<Data>> {
  operation(Data data, std::function<void()> fn, scheduler<Data>& sched,
            op_id id);
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

static thread_local void* this_op;

}  // namespace ctx
