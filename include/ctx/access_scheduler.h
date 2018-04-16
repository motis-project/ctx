#pragma once

#include <mutex>
#include <vector>

#include "ctx/operation.h"
#include "ctx/scheduler.h"

namespace ctx {

template <typename Data>
struct access_scheduler : public scheduler<Data> {
  using op_ptr = std::shared_ptr<operation<Data>>;

  struct read {
    explicit read(access_scheduler& ctrl) : ctrl_{ctrl} { ctrl_.start_read(); }
    ~read() { ctrl_.end_read(); }
    access_scheduler& ctrl_;
  };

  struct write {
    explicit write(access_scheduler& ctrl) : ctrl_{ctrl} {
      ctrl_.start_write();
    }
    ~write() { ctrl_.end_write(); }
    access_scheduler& ctrl_;
  };

  explicit access_scheduler(boost::asio::io_service& ios)
      : scheduler<Data>{ios} {};

  void start_read() {
    std::unique_lock<std::mutex> l{lock_, std::defer_lock_t{}};

  aquire:  // while loop not suitable because condition needs to be locked, to
    l.lock();
    if (write_active_) {
      auto const op = current_op<Data>();
      read_queue_.emplace_back(op->shared_from_this());
      l.unlock();
      op->suspend(/*finish = */ false);
      goto aquire;
    }

    ++read_count_;
  }

  void end_read() {
    std::unique_lock<std::mutex> l{lock_};
    --read_count_;
    if (read_count_ == 0 && !write_queue_.empty()) {
      unqueue(write_queue_);
    } else if (!read_queue_.empty()) {
      unqueue(read_queue_);
    }
  }

  void start_write() {
    std::unique_lock<std::mutex> l{lock_, std::defer_lock_t{}};

  aquire:  // while loop not suitable because condition needs to be locked, too
    l.lock();
    if (write_active_ || read_count_ != 0) {
      auto const op = current_op<Data>();
      read_queue_.emplace_back(op->shared_from_this());
      l.unlock();
      op->suspend(/*finish = */ false);
      goto aquire;
    }

    write_active_ = true;
  }

  void end_write() {
    std::unique_lock<std::mutex> l{lock_};
    write_active_ = false;
    if (!write_queue_.empty()) {
      unqueue(write_queue_);
    } else if (!read_queue_.empty()) {
      unqueue(read_queue_);
    }
  }

  void unqueue(std::vector<op_ptr>& queue) {
    this->enqueue(queue.front());
    queue.erase(begin(queue));
  }

  template <typename Fn>
  void enqueue_read(Data d, Fn&& fn, op_id id) {
    this->enqueue(d,
                  [fn, this]() {
                    read r{*this};
                    return fn();
                  },
                  id);
  }

  template <typename Fn>
  void enqueue_write(Data d, Fn&& fn, op_id id) {
    this->enqueue(d,
                  [fn, this]() {
                    write r{*this};
                    return fn();
                  },
                  id);
  }

  template <typename Fn>
  auto post_read(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post(d,
                                 [fn, this]() {
                                   read r{*this};
                                   return fn();
                                 },
                                 id);
  }

  template <typename Fn>
  auto post_void_read(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post(d,
                                 [fn, this]() {
                                   read r{*this};
                                   return fn();
                                 },
                                 id);
  }

  template <typename Fn>
  auto post_write(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post(d,
                                 [fn, this]() {
                                   write r{*this};
                                   return fn();
                                 },
                                 id);
  }

  template <typename Fn>
  auto post_void_write(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post(d,
                                 [fn, this]() {
                                   write r{*this};
                                   return fn();
                                 },
                                 id);
  }

  std::mutex lock_;
  std::vector<op_ptr> write_queue_;
  std::vector<op_ptr> read_queue_;
  size_t read_count_ = 0;
  bool write_active_ = false;
};

}  // namespace ctx