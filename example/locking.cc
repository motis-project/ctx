#include <algorithm>
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

  void start_read() {
    std::unique_lock<std::mutex> l{lock_};
    if (write_active_) {
      auto& op = current_op<controller*>();
      read_queue_.emplace_back(op.shared_from_this());
      l.unlock();
      op.suspend(/*finish = */ false);
    } else {
      ++read_count_;
    }
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
    std::unique_lock<std::mutex> l{lock_};
    if (write_active_ || read_count_ != 0) {
      auto& op = current_op<controller*>();
      read_queue_.emplace_back(op.shared_from_this());
      l.unlock();
      op.suspend(/*finish = */ false);
    } else {
      write_active_ = true;
    }
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
    enqueue(this,
            []() {

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
  std::vector<int> data;

  auto const read_op = [&data]() {
    int sum = 0;
    for (auto const& d : data) {
      sum += d;
    }
    printf("%d\n", sum);
  };

  auto const write_op = [&data]() {
    int sum = 0;
    for (auto const& d : data) {
      sum += d;
    }
    data.emplace_back(sum);
  };

  boost::asio::io_service ios;
  controller c(ios);
  for (int i = 0; i < 10000; ++i) {
    c.enqueue(read_op, op_id("read", "?", 0));
    c.enqueue(read_op, op_id("read", "?", 0));
    c.enqueue(read_op, op_id("read", "?", 0));
    c.enqueue(read_op, op_id("read", "?", 0));
    c.enqueue(read_op, op_id("read", "?", 0));
    c.enqueue(read_op, op_id("read", "?", 0));
    c.enqueue(read_op, op_id("read", "?", 0));
    c.enqueue(write_op, op_id("write", "?", 0));
  }

  constexpr auto const worker_count = 8;
  std::vector<std::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = std::thread([&]() { ios.run(); });
  }
  std::for_each(begin(threads), end(threads), [](std::thread& t) { t.join(); });
}
