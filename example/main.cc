#include <cstdio>
#include <vector>
#include <algorithm>
#include <chrono>

#include "boost/thread/thread.hpp"

#include "ctx/future.h"
#include "ctx/operation.h"
#include "ctx/scheduler.h"
#include "ctx/worker.h"

#define MOTIS_START_TIMING(_X) \
  auto _X##_start = std::chrono::steady_clock::now(), _X##_stop = _X##_start
#define MOTIS_STOP_TIMING(_X) _X##_stop = std::chrono::steady_clock::now()
#define MOTIS_TIMING_MS(_X)                                          \
  (std::chrono::duration_cast<std::chrono::milliseconds>(_X##_stop - \
                                                         _X##_start) \
       .count())
#define MOTIS_TIMING_US(_X)                                          \
  (std::chrono::duration_cast<std::chrono::microseconds>(_X##_stop - \
                                                         _X##_start) \
       .count())

using namespace ctx;

int main() {
  MOTIS_START_TIMING(total);

  scheduler sched;

  constexpr auto iterations = 100000;
  std::atomic<int> c(iterations);
  for (int i = 0; i < iterations; ++i) {
    sched.enqueue(new operation([&]() {
      // auto f = sched.post([&]() { return 5; });
      // printf("operation: %d\n", f->get());
      --c;
      if (c == 0) {
        sched.queue_.stop();
      }
    }, sched));
  }

  int worker_count = 1;
  std::vector<boost::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = boost::thread([&]() { worker(sched.queue_).run(); });
  }
  std::for_each(begin(threads), end(threads),
                [](boost::thread& t) { t.join(); });

  MOTIS_STOP_TIMING(total);
  printf("%lld\n", MOTIS_TIMING_MS(total));
}