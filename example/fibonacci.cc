#include <algorithm>
#include <thread>
#include <vector>

#include "ctx/ctx.h"

using namespace ctx;

struct simple_data {
  void transition(transition, op_id, op_id) {}
};

typedef scheduler<simple_data> scheduler_t;

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

int recfib_sync(int i) {
  if (i == 0) {
    return 0;
  }
  if (i == 1) {
    return 1;
  }

  return recfib_sync(i - 1) + recfib_sync(i - 2);
}

int recfib_async(int i) {
  if (i < 20) {
    return recfib_sync(i);
  }

  auto res_1 = ctx_call(simple_data(), std::bind(recfib_async, i - 1));
  auto res_2 = ctx_call(simple_data(), std::bind(recfib_async, i - 2));
  return res_1->val() + res_2->val();
}

void check(int n, int expected) {
  auto actual = ctx_call(simple_data(), std::bind(recfib_async, n))->val();

  if (actual == expected) {
    printf("fib result matched %d: %d\n", n, expected);
  } else {
    printf("fib result did not match %d: %d != %d\n", n, actual, expected);
  }
}

int main() {
  constexpr auto kCount = 40;

  std::vector<int> expected;
  for (int i = 0; i < kCount; ++i) {
    expected.push_back(iterfib(i));
  }

  boost::asio::io_service ios;
  scheduler_t sched(ios);
  for (int i = 0; i < kCount; ++i) {
    sched.enqueue(simple_data(), std::bind(check, i, expected[i]),
                  op_id("?", "?", 0));
  }

  int worker_count = 8;
  std::vector<std::thread> threads(worker_count);
  for (int i = 0; i < worker_count; ++i) {
    threads[i] = std::thread([&]() { ios.run(); });
  }
  std::for_each(begin(threads), end(threads), [](std::thread& t) { t.join(); });
}
