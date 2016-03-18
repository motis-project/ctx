#pragma once

#include <mutex>
#include <map>
#include <atomic>

#include "boost/lockfree/queue.hpp"

#include "ctx/op_id.h"

namespace ctx {

struct op_tracker {
  static constexpr auto kMaxQueueSize = 16384;

  enum class state { ready, running, waiting, finished };

  op_tracker() : log_(kMaxQueueSize) {}

  struct log_entry {
    state s;
    op_id caller, callee;
  };

  void log(state const s, op_id id1, op_id id2) {
    log_.push(log_entry{s, id1, id2});
    ++log_size_;
    if (log_size_ > kMaxQueueSize) {
      compact_log();
    }
  }

  void print_status() {
    std::lock_guard<std::mutex> lock(mutex_);

    printf("STATE:\n");
    for (auto const& op : status_) {
      printf("%u\t%s\t%s\t[%s]\n", op.first.index, op.first.name,
             status_to_string(op.second.first), op.first.created_at);
    }
    printf("\n");
  }

private:
  static char const* status_to_string(state s) {
    switch (s) {
      case state::ready: return "READY";
      case state::running: return "RUNNING";
      case state::waiting: return "WAITING";
      case state::finished: return "FINISHED";
      default: return "UNKNOWN";
    }
  }

  void compact_log() {
    std::lock_guard<std::mutex> lock(mutex_);

    log_.consume_all([this](log_entry const& e) {
      if (e.s != state::finished) {
        status_[e.caller] = std::make_pair(e.s, e.callee);
      } else {
        status_.erase(e.caller);
      }
    });
  }

  std::map<op_id, std::pair<state, op_id>> status_;
  boost::lockfree::queue<log_entry> log_;
  std::atomic<unsigned> log_size_;
  std::mutex mutex_;
};

}  // namespace ctx

