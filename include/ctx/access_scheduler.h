#pragma once

#include <utl/raii.h>
#include <cassert>
#include <map>
#include <mutex>
#include <variant>
#include <vector>

#include "utl/to_vec.h"

#include "ctx/access_request.h"
#include "ctx/access_t.h"
#include "ctx/operation.h"
#include "ctx/res_id_t.h"
#include "ctx/scheduler.h"

namespace ctx {

enum class op_type_t : uint8_t { IO, WORK };

template <typename Data, typename = void>
struct is_access_data : std::false_type {};

template <typename Data>
struct is_access_data<Data,
                      std::void_t<decltype(std::declval<Data>().res_access_)>>
    : std::true_type {};

template <typename T>
inline constexpr auto const is_access_data_v = is_access_data<T>::value;

template <typename Data>
struct access_scheduler : public scheduler<Data> {
  static_assert(is_access_data_v<Data>);

  struct queue_entry {
    op_type_t type_;
    std::shared_ptr<operation<Data>> op_;
  };

  struct res_state {
    bool finished() const {
      return active_writers_ == 0U && active_readers_ == 0U &&  //
             write_queue_.empty() && read_queue_.empty();
    }

    std::vector<queue_entry> write_queue_;
    std::vector<queue_entry> read_queue_;
    size_t usage_count_{0U};
    size_t active_readers_{0U};
    size_t active_writers_{0U};
  };

  void wait_for_access(op_type_t const op_type, accesses_t const& access) {
    auto l = std::unique_lock{lock_, std::defer_lock_t{}};
    auto const op = current_op<Data>();

    auto const get_existing_access = [&](res_id_t const id) {
      if (auto const it = op->data_.res_access_.find(id);
          it != end(op->data_.res_access_)) {
        return it->second;
      } else {
        return access_t::NONE;
      }
    };

    auto const obtain_access = [&]() {
      for (access_request const& wants : access) {
        // case | has already | wants | require
        // ---- | ------------+-------+---------
        //    1 | READ        | READ  | -
        //    2 | WRITE       | *     | -
        //    3 | READ        | WRITE | active_readers == 1
        //    4 | NONE        | READ  | active_writers == 0
        //    5 | NONE        | WRITE | active_writers == active_readers == 0
        auto const has = get_existing_access(wants.res_id_);

        if (has == access_t::WRITE ||
            (has == access_t::READ && wants.access_ == access_t::READ)) {
          // Handles: cases 1, 2
          continue;
        } else {
          auto& res_s = state_[wants.res_id_];

          // Handles: case 3 (upgrade READ -> WRITE)
          if (has == access_t::READ && wants.access_ == access_t::WRITE) {
            assert(res_s.active_writers_ == 0U);
            assert(res_s.active_readers_ >= 1U);
            if (res_s.active_readers_ > 1) {
              // Other readers -> we need to wait for them to finish.
              return false;
            }
            continue;
          }

          // Handles: cases 4, 5 (no access -> READ or WRITE)
          if (wants.access_ == access_t::READ) {
            if (res_s.active_writers_ != 0U) {
              res_s.read_queue_.emplace_back(
                  queue_entry{op_type, op->shared_from_this()});
              return false;
            }
          } else {
            if (res_s.active_writers_ != 0U || res_s.active_readers_ != 0U) {
              res_s.write_queue_.emplace_back(
                  queue_entry{op_type, op->shared_from_this()});
              return false;
            }
          }
        }
      }
      return true;
    };

    auto const activate = [&]() {
      for (access_request const& a : access) {
        auto& res_access = op->data_.res_access_[a.res_id_];
        res_access = std::max(a.access_, res_access);

        auto& res_s = state_[a.res_id_];
        if (a.access_ == access_t::READ) {
          ++res_s.active_readers_;
        } else {
          ++res_s.active_writers_;
        }
      }
    };

    {
      auto const tmp = std::lock_guard{lock_};
      for (access_request const& a : access) {
        ++state_[a.res_id_].usage_count_;
      }
    }

    while (true) {
      l.lock();

      // Check access.
      if (obtain_access()) {
        activate();  // Success - activate.
        break;
      }

      // Not successful.
      // Wait until resumed by another operation.
      l.unlock();
      op->suspend(/*finish = */ false);
    }
  }

  void end_access(accesses_t const& access) {
    auto const l = std::unique_lock{lock_};
    for (access_request const& a : access) {
      auto& res_s = state_[a.res_id_];
      --res_s.usage_count_;

      if (res_s.usage_count_ == 0U) {
        state_.erase(a.res_id_);
        continue;
      }

      if (a.access_ == access_t::READ) {
        --res_s.active_readers_;
        if (res_s.active_readers_ == 0U && !res_s.write_queue_.empty()) {
          unqueue(res_s.write_queue_);
        } else if (!res_s.read_queue_.empty()) {
          unqueue(res_s.read_queue_);
        }
      } else {
        --res_s.active_writers_;
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
               accesses_t&& access) {
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
