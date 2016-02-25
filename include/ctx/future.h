#pragma once

#include <exception>
#include <atomic>

namespace ctx {

template <>
struct future {
  future() {}

  T& get() {
    cv_.wait([&]() { return result_available_; });
    if (exception_) {
      std::rethrow_exception(exception_);
    } else {
      return result_;
    }
  }

  void set(T&& result, std::exception_ptr exception) {
    {
      std::lock_guard<std::mutex> lock(cv_.state_.mutex);
      result_ = std::move(result);
      exception_ = std::move(exception);
      result_available_ = true;
    }
    cv_.notify();
  }

  T result_;
  std::exception_ptr exception_;
  condition_variable cv_;
  bool result_available_;
};

// namespace ctx