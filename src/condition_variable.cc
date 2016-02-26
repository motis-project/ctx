#include "ctx/condition_variable.h"

#include "ctx/operation.h"

namespace ctx {

condition_variable::condition_variable() : op_(operation::this_op) {}

void condition_variable::wait() { op_->suspend(); }

void condition_variable::notify() { op_->sched_.enqueue(op_); }

}  // namespace ctx