#include <cstdio>
#include <vector>
#include <algorithm>

#include "boost/thread/thread.hpp"

#include "ctx/operation.h"
#include "ctx/scheduler.h"
#include "ctx/worker.h"

using namespace ctx;

int main() {
  scheduler sched;
  auto op = std::make_shared<operation>([]() { printf("operation\n"); }, sched);
  sched.queue_[sched.queue_.any] << op;

  std::vector<boost::thread> threads(4);
  for (int i = 0; i < 4; ++i) {
    threads[i] = boost::thread([&]() { worker(sched.queue_).run(); });
  }
  std::for_each(begin(threads), end(threads),
                [](boost::thread& t) { t.join(); });
}