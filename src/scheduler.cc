#include "ctx/scheduler.h"

#include "ctx/operation.h"

namespace ctx {

void scheduler::enqueue(std::function<void()> fn) {
  enqueue(std::make_shared<operation>(std::move(fn), *this));
}

void scheduler::enqueue(std::shared_ptr<operation> op) {
  ios_.post([op]() { op->resume(); });
}

void scheduler::enqueue_initial(std::function<void()> fn) {
  enqueue(std::make_shared<operation>(fn, *this));
}

}  // namespace ctx
