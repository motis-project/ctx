#include "ctx/scheduler.h"

#include "ctx/operation.h"

namespace ctx {

void scheduler::enqueue(std::function<void()> fn, op_id id) {
  id.index = ++next_id_;
  enqueue(std::make_shared<operation>(std::move(fn), *this, std::move(id)));
}

void scheduler::enqueue(std::shared_ptr<operation> op) {
  on_ready(op->id_);
  ios_.post([op]() { op->resume(); });
}

void scheduler::on_resume(op_id const& id) {
  tracker_.log(op_tracker::state::running, id, id);
}

void scheduler::on_suspend(op_id const& id, op_id const& waiting_for) {
  tracker_.log(op_tracker::state::waiting, id, waiting_for);
}

void scheduler::on_finish(op_id const& id) {
  tracker_.log(op_tracker::state::finished, id, id);
}

void scheduler::on_ready(op_id const& id) {
  tracker_.log(op_tracker::state::ready, id, id);
}

}  // namespace ctx
