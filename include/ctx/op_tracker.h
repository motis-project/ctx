#pragma once

#include <mutex>
#include <map>
#include <atomic>

#include "boost/lockfree/queue.hpp"

#include "ctx/op_id.h"

namespace ctx {

struct op_tracker {
  static constexpr auto kMaxQueueSize = 16384;

  struct state {
    enum class client_state { NONE, RUN, WAIT };
    enum class ctx_state { NONE, QUEUED, ACTIVE, INACTIVE };

    state() : c_state(client_state::RUN), op_state(ctx_state::QUEUED) {}

    void transition(transition t, op_id const& callee) {
      switch (t) {
        case transition::RESUME: c_state = client_state::RUN; break;
        case transition::SUSPEND:
          c_state = client_state::WAIT;
          callee_ = callee;
          break;
        case transition::ENQUEUE: op_state = ctx_state::QUEUED; break;
        case transition::ACTIVATE: op_state = ctx_state::ACTIVE; break;
        case transition::DEACTIVATE: op_state = ctx_state::INACTIVE; break;
        default: assert(false);
      }
    }

    char const* get_client_state() const {
      switch (c_state) {
        case client_state::NONE: return "NONE";
        case client_state::RUN: return " RUN";
        case client_state::WAIT: return "WAIT";
      }
    }

    char const* get_operation_state() const {
      switch (op_state) {
        case ctx_state::NONE: return "NONE    ";
        case ctx_state::QUEUED: return "QUEUED  ";
        case ctx_state::ACTIVE: return "ACTIVE  ";
        case ctx_state::INACTIVE: return "INACTIVE";
      }
    }

    char const* waiting_for() const {
      return c_state == client_state::WAIT ? callee_.created_at : "";
    }

    op_id callee_;
    client_state c_state;
    ctx_state op_state;
  };

  op_tracker() : log_(kMaxQueueSize) {}

  struct log_entry {
    transition t;
    op_id caller, callee;
  };

  void transition(transition t, op_id id1, op_id id2) {
    log_.push(log_entry{t, id1, id2});
    ++log_size_;
    if (log_size_ >= kMaxQueueSize) {
      compact_log();
    }
  }

  void print_status() {
    std::lock_guard<std::mutex> lock(mutex_);

    printf("STATE:\n");
    for (auto const& op : status_) {
      printf("%010u\t%s\t%s|%s\t%s\t%s\n", op.first.index, op.first.name,
             op.second.get_client_state(), op.second.get_operation_state(),
             op.first.created_at, op.second.waiting_for());
    }
    printf("\n");
  }

private:
  void compact_log() {
    std::lock_guard<std::mutex> lock(mutex_);

    log_.consume_all([this](log_entry const& e) {
      if (e.t != transition::FIN) {
        status_[e.caller].transition(e.t, e.callee);
      } else {
        status_.erase(e.caller);
      }
    });
  }

  std::map<op_id, state> status_;
  boost::lockfree::queue<log_entry> log_;
  std::atomic<unsigned> log_size_;
  std::mutex mutex_;
};

}  // namespace ctx
