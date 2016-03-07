#pragma once

#include <memory>
#include <atomic>

#include "boost/asio/io_service.hpp"

#include "ctx/op_tracker.h"
#include "ctx/stack_manager.h"
#include "ctx/future.h"
#include "ctx/op_id.h"

namespace ctx {

struct operation;

struct scheduler {
  scheduler() : next_id_(0) {}
  scheduler(scheduler const&) = delete;
  scheduler& operator=(scheduler const&) = delete;

  template <typename Fn>
  auto post(Fn fn, op_id id) {
    using return_t = decltype(fn());
    auto f = std::make_shared<future<return_t>>();
    enqueue(std::function<void()>([f, fn]() {
              std::exception_ptr ex;
              try {
                f->set(fn());
                return;
              } catch (...) {
                ex = std::current_exception();
              }
              f->set(ex);
            }),
            std::move(id));
    return f;
  }

  template <typename Fn>
  auto post_void(Fn fn, op_id id) {
    auto f = std::make_shared<future<void>>();
    enqueue(std::function<void()>([f, fn]() {
              std::exception_ptr ex;
              try {
                fn();
                f->set();
                return;
              } catch (...) {
                ex = std::current_exception();
              }
              f->set(ex);
            }),
            std::move(id));
    return f;
  }

  void enqueue(std::function<void()> fn, op_id id);
  void enqueue(std::shared_ptr<operation> op);
  void enqueue_initial(std::function<void()> fn);

  void on_resume(op_id const& id);
  void on_suspend(op_id const& id, op_id const& waiting_for);
  void on_finish(op_id const& id);
  void on_ready(op_id const& id);

  std::atomic<unsigned> next_id_;
  boost::asio::io_service ios_;
  stack_manager stack_manager_;
  op_tracker tracker_;
};

}  // namespace ctx
