#pragma once

#include <utl/raii.h>
#include <map>
#include <mutex>
#include <variant>
#include <vector>

#include "utl/to_vec.h"

#include "ctx/access_t.h"
#include "ctx/operation.h"

namespace ctx {

enum class op_type_t : uint8_t { IO, WORK };

struct access_request {
  uint64_t res_id_{0U};
  access_t access_{access_t::READ};
};

template <typename Data>
struct access_scheduler : public scheduler<Data> {
  using res_id_t = uint64_t;

  struct queue_entry {
    op_type_t type_;
    std::shared_ptr<operation<Data>> op_;
  };

  struct res_state {
    bool finished() const {
      return !write_active_ && read_count_ == 0U &&  //
             write_queue_.empty() && read_queue_.empty();
    }

    std::vector<queue_entry> write_queue_;
    std::vector<queue_entry> read_queue_;
    size_t usage_count_{0U};
    size_t read_count_{0U};
    bool write_active_{false};
  };

  void wait_for_access(op_type_t const op_type,
                       std::vector<access_request> const& access) {
    auto l = std::unique_lock{lock_, std::defer_lock_t{}};
    auto const op = current_op<Data>();

    auto const get_access = [&]() {
      for (access_request const& a : access) {
        auto& res_s = state_[a.res_id_];
        if (a.access_ == access_t::READ) {
          if (res_s.write_active_) {
            res_s.read_queue_.emplace_back(
                queue_entry{op_type, op->shared_from_this()});
            return false;
          }
        } else {
          if (res_s.write_active_ || res_s.read_count_ != 0U) {
            res_s.write_queue_.emplace_back(
                queue_entry{op_type, op->shared_from_this()});
            return false;
          }
        }
      }
      return true;
    };

    auto const activate = [&]() {
      for (access_request const& a : access) {
        auto& res_s = state_[a.res_id_];
        if (a.access_ == access_t::READ) {
          ++res_s.read_count_;
        } else {
          res_s.write_active_ = true;
        }
      }
    };

    {
      auto const tmp = std::lock_guard{lock_};
      for (access_request const& a : access) {
        ++state_[a.res_id_].usage_count_;
      }
    }

  acquire:
    l.lock();
    if (!get_access()) {
      l.unlock();
      op->suspend(/*finish = */ false);
      goto acquire;
    }

    activate();
  }

  void end_access(std::vector<access_request> const& access) {
    auto const l = std::unique_lock{lock_};
    for (access_request const& a : access) {
      auto& res_s = state_[a.res_id_];
      --res_s.usage_count_;

      if (res_s.usage_count_ == 0U) {
        state_.erase(a.res_id_);
        continue;
      }

      if (a.access_ == access_t::READ) {
        --res_s.read_count_;
        if (res_s.read_count_ == 0U && !res_s.write_queue_.empty()) {
          unqueue(res_s.write_queue_);
        } else if (!res_s.read_queue_.empty()) {
          unqueue(res_s.read_queue_);
        }
      } else {
        res_s.write_active_ = false;
        if (!res_s.write_queue_.empty()) {
          unqueue(res_s.write_queue_);
        } else if (!res_s.read_queue_.empty()) {
          unqueue(res_s.read_queue_);
        }
      }
    }
  }

  void unqueue(std::vector<queue_entry>& queue) {
    auto& entry = queue.front();
    entry.type_ == op_type_t::IO ? this->enqueue_io(entry.op_)
                                 : this->enqueue_work(entry.op_);
    queue.erase(begin(queue));
  }

  template <typename Fn>
  void enqueue(Data&& d, Fn&& fn, op_id const id, op_type_t const op_type,
               std::vector<access_request>&& access) {
    if (access.empty()) {
      (op_type == op_type_t::IO)
          ? this->enqueue_io(d, std::forward<Fn>(fn), id)
          : this->enqueue_work(d, std::forward<Fn>(fn), id);
    } else {
      auto f = [fn = std::forward<Fn>(fn), access = std::move(access), op_type,
                this]() {
        wait_for_access(op_type, access);
        UTL_FINALLY([&]() { end_access(access); });
        return fn();
      };
      (op_type == op_type_t::IO) ? this->enqueue_io(d, std::move(f), id)
                                 : this->enqueue_work(d, std::move(f), id);
    }
  }

  std::mutex lock_;
  std::map<res_id_t, res_state> state_;
};

}  // namespace ctx
