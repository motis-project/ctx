#pragma once

#include <utl/raii.h>
#include <atomic>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>

#include "utl/to_vec.h"
#include "utl/verify.h"

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
inline constexpr auto const is_access_state_v = is_access_data<T>::value;

template <typename Data>
struct access_scheduler : public scheduler<Data> {
  static_assert(is_access_state_v<Data>);

  struct queue_entry {
    op_type_t type_;
    std::shared_ptr<operation<Data>> op_;
  };

  struct res_state {
    struct res_holder {
      explicit res_holder(res_id_t const res_id) : res_id_{res_id} {}
      res_holder(res_holder const&) = delete;
      res_holder(res_holder&&) = delete;
      res_holder& operator=(res_holder const&) = delete;
      res_holder& operator=(res_holder&&) = delete;

      virtual ~res_holder() = 0;
      virtual void* get() const = 0;

      res_id_t res_id_;
    };

    template <typename T>
    struct template_res_holder : public res_holder {
      template_res_holder(access_scheduler& s, res_id_t const res_id, T&& t)
          : res_holder{res_id}, scheduler_{s}, res_{std::move(t)} {}

      template_res_holder(template_res_holder const&) = delete;
      template_res_holder(template_res_holder&&) = delete;
      template_res_holder& operator=(template_res_holder const&) = delete;
      template_res_holder& operator=(template_res_holder&&) = delete;

      ~template_res_holder() override {
        auto lock = std::lock_guard{scheduler_.lock_};
        scheduler_.state_.erase(res_id_);
      }

      void* get() override { return &res_; }

      access_scheduler& scheduler_;
      T res_;
    };

    template <typename T>
    res_state(access_scheduler& s, res_id_t const res_id, T&& res)
        : holder_{std::make_shared<template_res_holder>(s, res_id, res)} {}

    bool finished() const {
      return active_writers_ == 0U && active_readers_ == 0U &&  //
             write_queue_.empty() && read_queue_.empty();
    }

    std::vector<queue_entry> write_queue_;
    std::vector<queue_entry> read_queue_;
    size_t usage_count_{0U};
    size_t active_readers_{0U};
    size_t active_writers_{0U};

    // Memory management
    // =================
    //
    // When adding a resource:
    //   - create shared_ptr and weak_ptr
    //
    // When deleting a resource:
    //   - Delete shared_ptr and leave weak_ptr intact. It could be used
    //     by follow-up operations of operations that currently use the resource
    //     and have another instance of the shared_ptr - so the weak_ptr still
    //     works and can be used to finish those operations.
    //   - In the shared_ptr destructor (called after the last operation
    //     destroyed its locked shared_ptr instance) the complete entry
    //     will be removed from the map. This is after the last operation that
    //     was using it or waiting to use it has finished.
    //
    // When creating an operation that wants to read/write this resource:
    //   - Create a lock from the weak_ptr (the shared_ptr might be set to
    //     nullptr if delete_resource was called from outside), keep the
    //     shared_ptr as local variable on the operations stack, so it stays
    //     locked as long as the operation has to wait (suspended) to gain
    //     access.
    //   - When the operation's user function gets activated, the shared_ptr
    //     is handed over to the user.
    using deleter = std::function<void(void*)>;
    std::shared_ptr<res_holder> holder_;
    std::weak_ptr<res_holder> weak_;
  };

  struct mutex {
    mutex(access_scheduler& s, op_type_t const op_type, accesses_t access)
        : s_{s}, access_{std::move(access)} {
      wait_for_access(op_type);
    }

    mutex(mutex const&) = delete;
    mutex(mutex&&) = delete;
    mutex& operator=(mutex const&) = delete;
    mutex& operator=(mutex&&) = delete;

    ~mutex() { end_access(); }

    void wait_for_access(op_type_t const op_type) {
      auto l = std::unique_lock{s_.lock_, std::defer_lock_t{}};
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
        for (access_request const& wants : access_) {
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
            auto& res_s = s_.state_.at(wants.res_id_);

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
        for (access_request const& a : access_) {
          auto& res_access = op->data_.res_access_[a.res_id_];
          res_access = std::max(a.access_, res_access);

          auto& res_s = s_.state_.at(a.res_id_);
          if (a.access_ == access_t::READ) {
            ++res_s.active_readers_;
          } else {
            ++res_s.active_writers_;
          }
        }
      };

      {
        auto const tmp = std::lock_guard{s_.lock_};
        for (access_request const& a : access_) {
          ++s_.state_.at(a.res_id_).usage_count_;
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

    void end_access() {
      auto const l = std::unique_lock{s_.lock_};
      for (access_request const& a : access_) {
        auto& res_s = s_.state_.at(a.res_id_);
        --res_s.usage_count_;

        if (res_s.usage_count_ == 0U) {
          s_.state_.erase(a.res_id_);
          continue;
        }

        if (a.access_ == access_t::READ) {
          --res_s.active_readers_;
          if (res_s.active_readers_ == 0U && !res_s.write_queue_.empty()) {
            s_.unqueue(res_s.write_queue_);
          } else if (!res_s.read_queue_.empty()) {
            s_.unqueue(res_s.read_queue_);
          }
        } else {
          --res_s.active_writers_;
          if (!res_s.write_queue_.empty()) {
            s_.unqueue(res_s.write_queue_);
          } else if (!res_s.read_queue_.empty()) {
            s_.unqueue(res_s.read_queue_);
          }
        }
      }
    }

    access_scheduler& s_;
    accesses_t access_;
  };

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
                this]() mutable {
        auto lock = mutex{*this, op_type, std::move(access)};
        return fn();
      };
      (op_type == op_type_t::IO) ? this->enqueue_io(d, std::move(f), id)
                                 : this->enqueue_work(d, std::move(f), id);
    }
  }

  template <typename T>
  void emplace_data(ctx::res_id_t const res_id, T&& t) {
    auto const it = state_.find(res_id);
    utl::verify(it != end(state_), "{} not in shared_data", res_id);
    state_.emplace(res_id, *this, res_id, std::forward<T>(t));
  }

  bool includes(ctx::res_id_t const res_id) const {
    return state_.find(res_id) != end(state_);
  }

  template <typename T>
  T const& get(ctx::res_id_t const res_id) const {
    auto const it = state_.find(res_id);
    utl::verify(it != end(state_), "{} not in shared_data", res_id);
    return *reinterpret_cast<T const*>(it->second.get());
  }

  template <typename T>
  T& get(ctx::res_id_t const res_id) {
    auto const it = state_.find(res_id);
    utl::verify(it != end(state_), "{} not in shared_data", res_id);
    return *reinterpret_cast<T*>(it->second.get());
  }

  template <typename T>
  T const* find(ctx::res_id_t const res_id) const {
    auto const it = state_.find(res_id);
    return it != end(state_) ? reinterpret_cast<T const*>(it->second.get())
                             : nullptr;
  }

  std::mutex lock_;
  std::map<res_id_t, res_state> state_;
  std::atomic<ctx::res_id_t> res_id_;
};

}  // namespace ctx
