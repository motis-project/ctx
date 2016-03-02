#include <vector>
#include <algorithm>

#include "boost/thread/thread.hpp"

#include "ctx/future.h"
#include "ctx/operation.h"
#include "ctx/scheduler.h"

using namespace ctx;

int iterfib(int count) {
  if (count == 0) {
    return 0;
  }
  if (count == 1) {
    return 1;
  }

  int i = 0;
  int j = 1;

  for (int c = 0; c < count - 1; ++c) {
    int tmp = j;
    j = i + j;
    i = tmp;
  }
  return j;
}

int recfib(int i) {
  if (i == 0) {
    return 0;
  }
  if (i == 1) {
    return 1;
  }

  auto res_1 = operation::this_op->call(std::bind(recfib, i - 1));
  auto res_2 = operation::this_op->call(std::bind(recfib, i - 2));
  return res_1->val() + res_2->val();
}

void check(int n, int expected) {
  auto actual = operation::this_op->call(std::bind(recfib, n))->val();

  if (actual == expected) {
    printf("fib result matched %d: %d\n", n, expected);
  } else {
    printf("fib result did not match %d: %d != %d\n", n, actual, expected);
  }
}

int main() {
  constexpr auto kCount = 30;

  std::vector<int> expected;
  for (int i = 0; i < kCount; ++i) {
    expected.push_back(iterfib(i));
  }

  scheduler sched;
  for (int i = 0; i < kCount; ++i) {
    sched.enqueue(
        std::make_shared<operation>(std::bind(check, i, expected[i]), sched));
  }

  int worker_count = 8;
  std::vector<boost::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = boost::thread([&]() { sched.ios_.run(); });
  }
  std::for_each(begin(threads), end(threads),
                [](boost::thread& t) { t.join(); });
}