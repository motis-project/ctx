#pragma once

#include <atomic>
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

    void clear() {
      std::lock_guard g(lock_);
      stack_ = std::stack<fn>{};
    }

    std::mutex lock_;
    std::stack<fn> stack_;
  };

  explicit runner(boost::asio::io_service& ios) : ios_{ios} {}

  void run() {
    while (!stop_ && ios_.run_one()) {
      execute_work_tasks();
      while (!stop_ && ios_.poll_one())
        ;
    }
  }

  void reset() {
    ios_.reset();
    work_stack_.clear();
    stop_ = false;
  }

  void execute_work_tasks() {
    while (!stop_) {
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

  void stop() {
    stop_ = true;
    ios_.stop();
  }
  concurrent_stack work_stack_;
  boost::asio::io_service& ios_;
  std::atomic_bool stop_{false};
};

}  // namespace ctx