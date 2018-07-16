#pragma once

#include <functional>
#include <thread>

#include "boost/asio/io_service.hpp"

#include "ctx/concurrent_stack.h"

namespace ctx {

struct runner {
  void stop() { ios_.stop(); }

  boost::asio::io_service& ios() { return ios_; }

  void run(unsigned thread_count) {
    std::vector<std::thread> workers{thread_count};
    for (auto& w : workers) {
      w = std::thread([&]() {
        while (true) {
          if (auto f = work_stack_.poll(); f.has_value()) {
            (*f)();
          } else {
            return;
          }
        }
      });
    }

    while (true) {
      try {
        ios_.run();
        work_stack_.stop();
        break;
      } catch (std::exception const& e) {
        printf("unhandled error: %s\n", e.what());
      } catch (...) {
        printf("unhandled unknown error");
      }
      ios_.reset();
      work_stack_.reset();
    }

    for (auto& w : workers) {
      w.join();
    }

    ios_.reset();
    work_stack_.reset();
  }

  template <typename Fn>
  void post_high_prio(Fn&& f) {
    work_stack_.push(std::forward<Fn>(f));
  }

  template <typename Fn>
  void post_low_prio(Fn&& f) {
    work_stack_.push_bottom(std::forward<Fn>(f));
  }

  boost::asio::io_service ios_;
  concurrent_stack<std::function<void()>> work_stack_;
};

}  // namespace ctx