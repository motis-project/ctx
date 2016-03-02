#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>
#include <chrono>

#include "boost/thread/thread.hpp"

#include "ctx/future.h"
#include "ctx/operation.h"
#include "ctx/scheduler.h"
#include "ctx/worker.h"

using namespace ctx;

struct randomness {
  precomputed_randomness(std::size_t num_values) : v_(num_values), i_(0) {
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
} random;

int run();

int run() {
  std::vector<future_ptr> futures;
  for (int i = 0; i < 100; ++i) {
    usleep(random.get() * 1000);
    auto f = operation::this_op->call(run);
    if (random.get() > 0.5) {
      futures.emplace_back(std::move(f));
    }
  }
  return std::accumulate(begin(futures), end(futures), 0,
                         [](future_ptr const& f) { return f.get(); });
}

int main() {
  randomness rand(100000);

  scheduler sched;
  constexpr auto iterations = 100000;
  std::atomic<int> c(iterations);
  for (int i = 0; i < iterations; ++i) {
    sched.enqueue(std::make_shared<operation>(run, sched));
  }

  int worker_count = 10;
  std::vector<boost::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = boost::thread([&]() { worker(sched.queue_).run(); });
  }
  std::for_each(begin(threads), end(threads),
                [](boost::thread& t) { t.join(); });
}