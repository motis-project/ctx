#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "ctx/ctx.h"

using namespace ctx;

struct simple_data {
  void transition(transition, op_id, op_id) {}
};

int main() {
  std::vector<uint64_t> data({1, 1});

  std::mutex cout_mutex;

  auto const read_op = [&data, &cout_mutex]() {
    uint64_t sum = 0;
    for (auto const& d : data) {
      sum += d;
    }
    if (sum != 0 && sum % 1000 == 0) {
      std::lock_guard<std::mutex> l{cout_mutex};
      std::cout << sum << "\n";
    }
  };

  auto const write_op = [&data]() {
    data.emplace_back(data[data.size() - 1] + data[data.size() - 2]);
  };

  boost::asio::io_service ios;
  access_scheduler<simple_data> c(ios);
  for (int i = 0; i < 2000; ++i) {
    c.enqueue_read(simple_data{}, read_op, op_id("read", "?", 0));
    c.enqueue_read(simple_data{}, read_op, op_id("read", "?", 0));
    c.enqueue_read(simple_data{}, read_op, op_id("read", "?", 0));
    c.enqueue_read(simple_data{}, read_op, op_id("read", "?", 0));
    c.enqueue_read(simple_data{}, read_op, op_id("read", "?", 0));
    c.enqueue_read(simple_data{}, read_op, op_id("read", "?", 0));
    c.enqueue_read(simple_data{}, read_op, op_id("read", "?", 0));
    c.enqueue_write(simple_data{}, write_op, op_id("write", "?", 0));
  }

  constexpr auto const worker_count = 4;
  std::vector<std::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = std::thread([&]() { ios.run(); });
  }
  std::for_each(begin(threads), end(threads), [](std::thread& t) { t.join(); });

  std::cout << data.back() << "\n";
}
