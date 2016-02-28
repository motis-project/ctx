#include "ctx/condition_variable.h"

#include <cassert>

#include "ctx/operation.h"

namespace ctx {

condition_variable::condition_variable()
    : caller_(operation::this_op->shared_from_this()) {}

void condition_variable::wait() {
  auto caller_lock = caller_.lock();
  assert(caller_lock);
  caller_lock->suspend(/* finished = */ false);
}

void condition_variable::notify() {
  auto caller_lock = caller_.lock();
  if (caller_lock) {
    caller_lock->sched_.enqueue(caller_lock);
  }
}

}  // namespace ctx