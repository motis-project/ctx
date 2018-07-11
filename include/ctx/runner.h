#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <stack>

#include "boost/asio/io_service.hpp"

namespace ctx {

struct runner {
  struct concurrent_stack {
    using fn = std::function<void()>;

    std::optional<fn> pop() {
      std::lock_guard g(lock_);
      if (stack_.empty()) {
        return std::nullopt;
      } else {
        auto const r = std::move(stack_.top());
        stack_.pop();
        return r;
      }
    }

    template <typename T>
    void push(T&& f) {
      std::lock_guard g(lock_);
      stack_.emplace(std::forward<T>(f));
    }

    std::mutex lock_;
    std::stack<fn> stack_;
  };

  explicit runner(boost::asio::io_service& ios) : ios_{ios} {}

  void run() {
    while (ios_.run_one()) {
      execute_work_tasks();
      while (ios_.poll_one())
        ;
    }
  }

  void execute_work_tasks() {
    while (true) {
      if (auto f = work_stack_.pop(); f.has_value()) {
        (*f)();
      } else {
        break;
      }
    }
  }

  template <typename Fn>
  void post_work_task(Fn&& f) {
    work_stack_.push(std::forward<Fn>(f));
  }

  template <typename Fn>
  void post_io_task(Fn&& f) {
    ios_.post(std::forward<Fn>(f));
  }

  concurrent_stack work_stack_;
  boost::asio::io_service& ios_;
};

}  // namespace ctx