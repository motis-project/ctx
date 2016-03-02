#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <numeric>
#include <vector>
#include <algorithm>
#include <chrono>

#include "boost/thread/thread.hpp"

#include "ctx/future.h"
#include "ctx/operation.h"
#include "ctx/scheduler.h"
#include "test/fibonacci.h"

using namespace ctx;
using namespace test;

/*
struct randomness {
  randomness(std::size_t num_values) : v_(num_values), i_(0) {
    std::srand(std::time(0));
    std::generate(begin(v_), end(v_),
                  []() { return std::rand() / static_cast<double>(RAND_MAX); });
  }

  double get() {
    if (i_ >= v_.size()) {
      i_ = 0;
    }
    return v_[i_++];
  }

  std::vector<double> v_;
  std::size_t i_;
} gen(100000);

int run();

int run() {
  int sleep = gen.get() * 1000;
  std::vector<future_ptr<int>> futures;
  for (int i = 0; i < 10; ++i) {
    usleep(sleep);
    if (gen.get() > 0.9) {
      auto f = operation::this_op->call(run);
      // if (gen.get() > 0.5) {
        futures.emplace_back(std::move(f));
      // }
    }
  }
  int sum = sleep + std::accumulate(
                        begin(futures), end(futures), 0,
                        [](int acc, auto const& f) { return acc + f->get(); });
  printf("%d\n", sum);
  return sum;
}
*/

int main() {
  /*
  randomness rand(100000);

  scheduler sched;
  constexpr auto iterations = 3;
  std::atomic<int> c(iterations);
  for (int i = 0; i < iterations; ++i) {
    sched.enqueue(std::make_shared<operation>(run, sched));
  }
  */

  constexpr auto kCount = 30;

  std::vector<int> expected;
  for(int i = 0; i < kCount; ++i) {
    expected.push_back(iterfib(i));
  }

  scheduler sched;
  for (int i = 0; i < kCount; ++i) {
    sched.enqueue(std::make_shared<operation>(std::bind(check, i, expected[i]), sched));
  }


  int worker_count = 8;
  std::vector<boost::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = boost::thread([&]() { 
      sched.ios_.run();
    });
  }
  std::for_each(begin(threads), end(threads),
                [](boost::thread& t) { t.join(); });
}