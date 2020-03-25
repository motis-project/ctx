#include "ctx/parallel_for.h"

#include <iostream>
#include <numeric>
#include <vector>

using namespace ctx;

struct simple_data {
  void transition(transition, op_id, op_id) {}
};

int main() {
  std::vector<int> values(20);
  std::iota(values.begin(), values.end(), 0);

  std::cout << "before:\n";
  for (auto const& val : values) {
    std::cout << val << " ";
  }
  std::cout << "\n";

  std::cout << "process:\n";
  boost::asio::io_service ios;
  scheduler<simple_data> sched;
  sched.enqueue_work(
      simple_data(),
      [&] {
        parallel_for<simple_data>(values,
                                  [](int& value) {
                                    std::cout << value << " ";
                                    value *= value;
                                  },
                                  {});
      },
      op_id("?", "?", 0));

  int worker_count = 8;
  sched.run(worker_count);
  std::cout << "\n";

  std::cout << "after:\n";
  for (auto const& val : values) {
    std::cout << val << " ";
  }
  std::cout << "\n";
}
