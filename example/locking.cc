#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "ctx/ctx.h"

using namespace ctx;

struct controller {
  struct read {
    explicit read(controller& ctrl) : ctrl_{ctrl} { ctrl_.start_read(); }
    ~read() { ctrl_.end_read(); }
    controller& ctrl_;
  };

  struct write {
    explicit write(controller& ctrl) : ctrl_{ctrl} { ctrl_.start_write(); }
    ~write() { ctrl_.end_write(); }
    controller& ctrl_;
  };

  controller(boost::asio::io_service& ios) : scheduler_{ios} {};

  void start_read() {
    std::unique_lock<std::mutex> l{lock_, std::defer_lock_t{}};

  aquire:  // while loop not suitable because condition needs to be locked, to
    l.lock();
    if (write_active_) {
      auto& op = current_op<controller*>();
      read_queue_.emplace_back(op.shared_from_this());
      l.unlock();
      op.suspend(/*finish = */ false);
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
      auto& op = current_op<controller*>();
      read_queue_.emplace_back(op.shared_from_this());
      l.unlock();
      op.suspend(/*finish = */ false);
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

  void unqueue(std::vector<std::shared_ptr<operation<controller*>>>& queue) {
    scheduler_.enqueue(queue.front());
    queue.erase(begin(queue));
  }

  void transition(transition, op_id, op_id) {}

  template <typename Fn>
  void enqueue_read(Fn&& fn, op_id id) {
    scheduler_.enqueue(this,
                       [fn, this]() {
                         read r{*this};
                         fn();
                       },
                       id);
  }

  template <typename Fn>
  void enqueue_write(Fn&& fn, op_id id) {
    scheduler_.enqueue(this,
                       [fn, this]() {
                         write r{*this};
                         fn();
                       },
                       id);
  }

  scheduler<controller*> scheduler_;
  std::mutex lock_;
  std::vector<std::shared_ptr<operation<controller*>>> write_queue_;
  std::vector<std::shared_ptr<operation<controller*>>> read_queue_;
  size_t read_count_ = 0;
  bool write_active_ = false;
};

int main() {
  std::vector<uint64_t> data({1, 1});

  std::mutex m;

  auto const read_op = [&data, &m]() {
    uint64_t sum = 0;
    for (auto const& d : data) {
      sum += d;
    }
    if (sum != 0 && sum % 1000 == 0) {
      std::lock_guard<std::mutex> l{m};
      std::cout << sum << "\n";
    }
  };

  auto const write_op = [&data]() {
    data.emplace_back(data[data.size() - 1] + data[data.size() - 2]);
  };

  boost::asio::io_service ios;
  controller c(ios);
  for (int i = 0; i < 2000; ++i) {
    c.enqueue_read(read_op, op_id("read", "?", 0));
    c.enqueue_read(read_op, op_id("read", "?", 0));
    c.enqueue_read(read_op, op_id("read", "?", 0));
    c.enqueue_read(read_op, op_id("read", "?", 0));
    c.enqueue_read(read_op, op_id("read", "?", 0));
    c.enqueue_read(read_op, op_id("read", "?", 0));
    c.enqueue_read(read_op, op_id("read", "?", 0));
    c.enqueue_write(write_op, op_id("write", "?", 0));
  }

  constexpr auto const worker_count = 4;
  std::vector<std::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = std::thread([&]() { ios.run(); });
  }
  std::for_each(begin(threads), end(threads), [](std::thread& t) { t.join(); });

  std::cout << data.back() << "\n";
}
