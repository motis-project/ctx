#pragma once

#include <atomic>
#include <memory>

#include "boost/asio/io_service.hpp"

#include "ctx/future.h"
#include "ctx/op_id.h"
#include "ctx/runner.h"
#include "ctx/stack_manager.h"

namespace ctx {

template <typename Data>
struct scheduler {
  scheduler() = default;
  scheduler(scheduler const&) = delete;
  scheduler& operator=(scheduler const&) = delete;

  void run(unsigned num_threads) { runner_.run(num_threads); }

  unsigned next_op_id() { return ++next_id_; }

  template <typename Fn>
  auto post_io(Data data, Fn fn, op_id id);

  template <typename Fn>
  future_ptr<Data, void> post_void_io(Data data, Fn fn, op_id id);

  template <typename Fn>
  auto post_work(Data data, Fn fn, op_id id);

  template <typename Fn>
  future_ptr<Data, void> post_void_work(Data data, Fn fn, op_id id);

  void enqueue_io(Data, std::function<void()>, op_id);
  void enqueue_io(std::shared_ptr<operation<Data>> const&);

  void enqueue_work(Data, std::function<void()>, op_id);
  void enqueue_work(std::shared_ptr<operation<Data>> const&);

  std::atomic<unsigned> next_id_ = 0;
  runner runner_;
  stack_manager stack_manager_;
};

}  // namespace ctx
