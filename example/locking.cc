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

  access_scheduler<simple_data> c;
  for (int i = 0; i < 100; ++i) {
    c.enqueue(simple_data{}, read_op, op_id("read", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::READ}});
    c.enqueue(simple_data{}, read_op, op_id("read", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::READ}});
    c.enqueue(simple_data{}, read_op, op_id("read", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::READ}});
    c.enqueue(simple_data{}, read_op, op_id("read", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::READ}});
    c.enqueue(simple_data{}, read_op, op_id("read", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::READ}});
    c.enqueue(simple_data{}, read_op, op_id("read", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::READ}});
    c.enqueue(simple_data{}, read_op, op_id("read", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::READ}});
    c.enqueue(simple_data{}, write_op, op_id("write", "?", 0), op_type_t::WORK,
              {access_request{0U, ctx::access_t::WRITE}});
  }

  constexpr auto const worker_count = 4;
  c.run(worker_count);

  std::cout << data.back() << "\n";
  std::cout << c.state_.size() << "\n";
  for (auto const& [id, res] : c.state_) {
    std::cout << "id=" << id << ", usage_count=" << res.usage_count_ << "\n";
  }
}
