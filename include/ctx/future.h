#pragma once

#include <type_traits>
#include <exception>
#include <mutex>
#include <atomic>

#include "ctx/condition_variable.h"

namespace ctx {

template <typename T, typename Enable = void>
struct future {};

template <typename T>
struct future<T, typename std::enable_if<!std::is_same<T, void>::value>::type> {
  future() : result_available_(false) {}

  T& get() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&]() { return result_available_; });
    if (exception_) {
      std::rethrow_exception(exception_);
    } else {
      return result_;
    }
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
  std::exception_ptr exception_;
  std::mutex mutex_;
  condition_variable cv_;
  bool result_available_;
};

template <typename T>
struct future<T, typename std::enable_if<std::is_same<T, void>::value>::type> {
  future() {}

  void get() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&]() { return result_available_; });
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

  std::exception_ptr exception_;
  std::mutex mutex_;
  condition_variable cv_;
  bool result_available_;
};

template <typename T>
using future_ptr = std::shared_ptr<future<T>>;

}  // namespace ctx