#pragma once

#include <vector>
#include <limits>
#include <algorithm>

#include "boost/thread/mutex.hpp"
#include "boost/thread/condition_variable.hpp"

namespace ctx {

template <typename T>
class channel {
public:
  struct queue_element;

  typedef unsigned TopicId;
  typedef std::function<bool(queue_element)> callback_function;

  static constexpr TopicId any = std::numeric_limits<TopicId>::min();

  struct queue_element {
    queue_element() = default;
    queue_element(TopicId topic, T value)
        : topic(std::move(topic)), value(std::forward<T>(value)) {}
    TopicId topic;
    T value;
  };

  struct listener {
    listener(TopicId topic, callback_function callback)
        : topic(std::move(topic)), callback(std::move(callback)) {}
    TopicId topic;
    callback_function callback;
  };

  class topic {
  public:
    topic(channel& ch, TopicId id) : ch_(ch), id_(id) {}

    bool ready() {
      bool data_available = is_data_available();
      return data_available || (ch_.stopped_ && !data_available);
    }

    bool operator>>(queue_element& el) {
      boost::unique_lock<boost::mutex> synchronization(ch_.mutex_);

      if (!ch_.stopped_ && !is_data_available()) {
        ch_.cv_.wait(synchronization, std::bind(&topic::ready, this));
      }
      if (ch_.stopped_ && !is_data_available()) {
        return false;
      }

      auto it = next_data();
      el = *it;
      ch_.queue_.erase(it);
      ch_.cv_.notify_all();

      return true;
    }

    template <typename... Args>
    bool operator<<(Args... arg) {
      boost::unique_lock<boost::mutex> synchronization(ch_.mutex_);

      if (ch_.queue_.size() >= ch_.size_) {
        ch_.cv_.wait(synchronization,
                     [this]() { return ch_.queue_.size() < ch_.size_; });
      }

      if (ch_.stopped_) {
        return false;
      }

      ch_.queue_.emplace_back(id_, T(std::forward<T>(arg)...));
      ch_.cv_.notify_all();
      ch_.tell_listeners();

      return true;
    }

    typename std::vector<queue_element>::iterator next_data() {
      // return std::find_if(begin(ch_.queue_), end(ch_.queue_),
      //                     [this](queue_element const& el) {
      //                       return id_ == any || el.topic == id_;
      //                     });


      // return std::find_if(ch_.queue_.rbegin(), ch_.queue_.rend(),
      //                    [this](queue_element const& el) {
      //                      return id_ == any || el.topic == id_;
      //                    })
      //    .base();

      if(ch_.queue_.size() > 0) {
        return std::prev(ch_.queue_.end());
      } else {
        return ch_.queue_.end();
      }
    }



    bool is_data_available() { return next_data() != end(ch_.queue_); }

    void listen(callback_function cb) {
      boost::lock_guard<boost::mutex> synchronization(ch_.mutex_);
      ch_.listeners_.emplace_back(id_, std::move(cb));
      ch_.tell_listeners();
    }

  private:
    channel& ch_;
    TopicId id_;
  };

  channel(std::size_t size = std::numeric_limits<std::size_t>::max())
      : stopped_(false), size_(size) {}

  topic operator[](TopicId id) { return topic(*this, id); }

  void stop() {
    boost::lock_guard<boost::mutex> synchronization(mutex_);
    stopped_ = true;
    cv_.notify_all();
  }

  std::size_t size() const {
    boost::lock_guard<boost::mutex> synchronization(mutex_);
    return queue_.size();
  }

  void tell_listeners() {
    if (listeners_.empty()) return;

    auto it = begin(queue_);
    while (it != end(queue_)) {
      if (tell_listeners(*it)) {
        it = queue_.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  bool tell_listeners(queue_element const& el) {
    for (auto it = begin(listeners_); it != end(listeners_); ++it) {
      if (it->topic == any || it->topic == el.topic) {
        if (it->callback(el)) {
          listeners_.erase(it);
        }
        return true;
      }
    }
    return false;
  }

  mutable boost::mutex mutex_;
  boost::condition_variable cv_;
  std::vector<queue_element> queue_;
  std::vector<listener> listeners_;
  bool stopped_;
  std::size_t size_;
};

}  // namespace ctx