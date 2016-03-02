#pragma once

#include "ctx/operation.h"

using namespace ctx;

namespace test {

int iterfib(int count) {
  if(count == 0) {
    return 0;
  }
  if(count == 1) {
    return 1;
  }

  int i = 0;
  int j = 1;

  for(int c = 0; c < count-1; ++c) {
    int tmp = j;
    j = i +j;
    i = tmp;
  }
  return j;
}

int recfib(int i) {
  if(i == 0) {
    return 0;
  }
  if(i == 1) {
    return 1;
  }

  auto res_1 = operation::this_op->call(std::bind(recfib, i-1 ));
  auto res_2 = operation::this_op->call(std::bind(recfib, i-2 ));
  return res_1->val() + res_2->val();
}

void check(int n, int expected) {
  auto actual = operation::this_op->call(std::bind(recfib, n))->val();

  if(actual == expected) {
    printf("fib result matched %d: %d\n", n, expected);
  } else {
    printf("fib result did not match %d: %d != %d\n", n, actual, expected);
  }
}

} // namespace test
