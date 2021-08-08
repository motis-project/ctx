#pragma once

#include <atomic>
#include <exception>
#include <type_traits>

#include "ctx/condition_variable.h"
#include "ctx/op_id.h"
#include "ctx/operation.h"

namespace ctx {

template <typename Data, typename T, typename Enable = void>
struct future {};

template <typename Data, typename T>
struct future<Data, T,
              typename std::enable_if<!std::is_same<T, void>::value>::type> {
  future(op_id callee) : callee_(std::move(callee)), result_available_(false) {}

  T& val() {
    if (!result_available_) {
      current_op<Data>()->on_transition(transition::SUSPEND, callee_);
      cv_.wait([&]() { return result_available_.load(); });
      current_op<Data>()->on_transition(transition::RESUME);
    }
    if (exception_) {
      std::rethrow_exception(exception_);
    }
    return result_;
  }

  void set(T&& result) {
    result_ = std::move(result);
    result_available_.store(true);
    cv_.notify();
  }

  void set(std::exception_ptr e) {
    exception_ = e;
    result_available_.store(true);
    cv_.notify();
  }

  T result_;
  op_id callee_;
  std::exception_ptr exception_;
  condition_variable<Data> cv_;
  std::atomic_bool result_available_;
};

template <typename Data, typename T>
struct future<Data, T,
              typename std::enable_if<std::is_same<T, void>::value>::type> {
  future(op_id callee) : callee_(std::move(callee)), result_available_(false) {}

  void val() {
    if (!result_available_) {
      current_op<Data>()->on_transition(transition::SUSPEND, callee_);
      cv_.wait([&]() { return result_available_.load(); });
      current_op<Data>()->on_transition(transition::RESUME);
    }
    if (exception_) {
      std::rethrow_exception(exception_);
    }
  }

  void set() {
    result_available_.store(true);
    cv_.notify();
  }

  void set(std::exception_ptr e) {
    exception_ = e;
    result_available_.store(true);
    cv_.notify();
  }

  op_id callee_;
  std::exception_ptr exception_;
  condition_variable<Data> cv_;
  std::atomic_bool result_available_;
};

template <typename Data, typename T>
using future_ptr = std::shared_ptr<future<Data, T>>;

template <typename Data, typename T>
void await_all(std::vector<future_ptr<Data, T>> const& futures) {
  std::exception_ptr exception;
  for (auto const& fut : futures) {
    try {
      fut->val();
    } catch (...) {
      if (!exception) {
        exception = std::current_exception();
      }
    }
  }
  if (exception) {
    std::rethrow_exception(exception);
  }
}

}  // namespace ctx
