#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <stack>

namespace ctx {

template <typename T>
struct concurrent_stack {
  void stop() {
    std::lock_guard sync(lock_);
    stop_ = true;
    cv_.notify_all();
  }

  std::optional<T> poll() {
    std::unique_lock sync(lock_);
    cv_.wait(sync, [&]() { return stop_ || !data_.empty(); });
    if (stop_ && data_.empty()) {
      return std::nullopt;
    } else if (!data_.empty()) {
      return get_and_remove_top();
    }

    assert(false);
    return std::nullopt;
  }

  size_t size() {
    std::lock_guard sync(lock_);
    return data_.size();
  }

  void reset() {
    std::lock_guard sync(lock_);
    stop_ = false;
  }

  template <typename Arg>
  void push(Arg&& f) {
    std::lock_guard sync(lock_);
    data_.emplace_back(std::forward<T>(f));
    cv_.notify_all();
  }

  template <typename Arg>
  void push_bottom(Arg&& f) {
    std::lock_guard sync(lock_);
    data_.emplace(begin(data_), std::forward<T>(f));
    cv_.notify_all();
  }

private:
  T get_and_remove_top() {
    auto const r = std::move(data_.back());
    data_.erase(begin(data_) + data_.size() - 1);
    return r;
  }

  std::mutex lock_;
  std::condition_variable cv_;
  std::vector<T> data_;
  bool stop_ = false;
};

}  // namespace ctx