#pragma once

#include <memory>
#include <atomic>

#include "boost/asio/io_service.hpp"

#include "ctx/stack_manager.h"
#include "ctx/future.h"
#include "ctx/op_id.h"

namespace ctx {

template <typename Data>
struct scheduler {
  scheduler(boost::asio::io_service& ios) : next_id_(0), ios_(ios) {}

  scheduler(scheduler const&) = delete;
  scheduler& operator=(scheduler const&) = delete;

  unsigned next_op_id() { return ++next_id_; }

  template <typename Fn>
  auto post(Data data, Fn fn, op_id id);

  template <typename Fn>
  future_ptr<Data, void> post_void(Data data, Fn fn, op_id id);

  void enqueue(Data data, std::function<void()> fn, op_id id);

  void enqueue(std::shared_ptr<operation<Data>> const& op);

  std::atomic<unsigned> next_id_;
  boost::asio::io_service& ios_;
  stack_manager stack_manager_;
};

}  // namespace ctx
