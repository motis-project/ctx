#pragma once

#include <type_traits>
#include <exception>
#include <mutex>
#include <atomic>

#include "ctx/condition_variable.h"
#include "ctx/op_id.h"

namespace ctx {

template <typename Data, typename T, typename Enable = void>
struct future {};

template <typename Data, typename T>
struct future<Data, T,
              typename std::enable_if<!std::is_same<T, void>::value>::type> {
  future(op_id callee) : callee_(std::move(callee)), result_available_(false) {}

  T& val() {
    on_this_op_suspend<Data>(callee_);
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&]() { return result_available_; });
    on_this_op_resume<Data>();
    if (exception_) {
      std::rethrow_exception(exception_);
    }
    return result_;
  }

  void set(T&& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = std::move(result);
    result_available_ = true;
    cv_.notify();
  }

  void set(std::exception_ptr e) {
    std::lock_guard<std::mutex> lock(mutex_);
    exception_ = e;
    result_available_ = true;
    cv_.notify();
  }

  T result_;
  op_id callee_;
  std::exception_ptr exception_;
  std::mutex mutex_;
  condition_variable<Data> cv_;
  bool result_available_;
};

template <typename Data, typename T>
struct future<Data, T,
              typename std::enable_if<std::is_same<T, void>::value>::type> {
  future(op_id callee) : callee_(std::move(callee)), result_available_(false) {}

  void get() {
    on_this_op_suspend<Data>(callee_);
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&]() { return result_available_; });
    on_this_op_resume<Data>();
    if (exception_) {
      std::rethrow_exception(exception_);
    }
  }

  void set() {
    std::lock_guard<std::mutex> lock(mutex_);
    result_available_ = true;
    cv_.notify();
  }

  void set(std::exception_ptr e) {
    std::lock_guard<std::mutex> lock(mutex_);
    exception_ = e;
    result_available_ = true;
    cv_.notify();
  }

  op_id callee_;
  std::exception_ptr exception_;
  std::mutex mutex_;
  condition_variable<Data> cv_;
  bool result_available_;
};

template <typename Data, typename T>
using future_ptr = std::shared_ptr<future<Data, T>>;

}  // namespace ctx
