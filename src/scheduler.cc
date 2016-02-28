#include "ctx/scheduler.h"

#include "ctx/operation.h"

namespace ctx {

void scheduler::enqueue(std::function<void()> fn) {
  queue_[queue_.any] << std::make_shared<operation>(fn, *this);
}

}  // namespace ctx