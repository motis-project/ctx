#pragma once

#include <atomic>
#include <vector>

#include "ctx/ctx.h"

namespace ctx {

template <typename Data, typename T, typename Fn>
void parallel_for(T& vec, Fn fn, ctx::op_id id) {
  auto const op = ctx::current_op<Data>();
  id.parent_index = op->id_.index;

  std::atomic_bool has_execption{false};
  std::exception_ptr exception;

  std::vector<future_ptr<Data, void>> futures;
  for (auto& elem : vec) {
    auto wrapped = [&] {
      if (has_execption) {
        return;
      }
      fn(elem);
    };
    futures.push_back(op->sched_.post_void(op->data_, wrapped, id));
  }

  for (auto const& fut : futures) {
    try {
      fut->val();
    } catch (std::exception const& e) {
      if (!has_execption.exchange(true)) {
        exception = std::current_exception();
      }
    }
  }

  if (has_execption) {
    std::rethrow_exception(exception);
  }
}

}  // namespace ctx
