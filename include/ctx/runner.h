#pragma once

#include <functional>
#include <thread>

#include "fmt/base.h"

#include "utl/set_thread_name.h"

#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/io_context.hpp"

#include "ctx/concurrent_stack.h"

namespace ctx {

struct runner {
  void stop() { ios_.stop(); }

  boost::asio::io_context& ios() { return ios_; }

  void run(unsigned thread_count, bool quit_on_ios_exit = false) {
    ios_.restart();
    work_stack_.reset();

    auto work_guard = boost::asio::make_work_guard(ios_);
    if (quit_on_ios_exit) {
      work_guard.reset();
    }

    std::vector<std::thread> workers{thread_count};
    auto i = 0U;
    for (auto& w : workers) {
      w = std::thread([&]() {
        while (true) {
          if (auto f = work_stack_.poll(); f.has_value()) {
            (*f)();
            if (--elements_in_system_ == 0ul) {
              work_guard.reset();
            }
          } else {
            return;
          }
        }
      });
      utl::set_thread_name(w, fmt::format("motis worker {}", i++));
    }

    while (true) {
      try {
        ios_.run();
        work_stack_.stop();
        if (quit_on_ios_exit) {
          elements_in_system_ -= work_stack_.clear();
        }
        break;
      } catch (std::exception const& e) {
        printf("unhandled error: %s\n", e.what());
      } catch (...) {
        printf("unhandled unknown error");
      }
    }

    for (auto& w : workers) {
      w.join();
    }

    if (quit_on_ios_exit) {
      work_stack_.clear();
      elements_in_system_ = 0ul;
    }
  }

  template <typename Fn>
  void post_high_prio(Fn&& f) {
    ++elements_in_system_;
    work_stack_.push(std::forward<Fn>(f));
  }

  template <typename Fn>
  void post_low_prio(Fn&& f) {
    ++elements_in_system_;
    work_stack_.push_bottom(std::forward<Fn>(f));
  }

  boost::asio::io_context ios_;

private:
  concurrent_stack<std::function<void()>> work_stack_;
  std::mutex lock_;
  std::atomic<size_t> elements_in_system_ = 0ul;
};

}  // namespace ctx