#include "ctx/scheduler.h"

#include "ctx/operation.h"

namespace ctx {

void scheduler::enqueue(std::function<void()> fn) {
  enqueue(std::make_shared<operation>(fn, *this));
}

void scheduler::enqueue(std::shared_ptr<operation> op) {
  ios_.post([op]() { op->resume(); });
}

}  // namespace ctx