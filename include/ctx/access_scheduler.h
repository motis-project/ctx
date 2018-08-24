#pragma once

#include <mutex>
#include <vector>

#include "ctx/operation.h"
#include "ctx/scheduler.h"

namespace ctx {

enum class op_type_t { IO, WORK };
enum class access_t { NONE, READ, WRITE };

template <typename Data>
struct access_scheduler : public scheduler<Data> {
  struct queue_entry {
    using op = std::shared_ptr<operation<Data>>;
    queue_entry(op_type_t type, op const& op) : type_{type}, op_{op} {}
    op_type_t type_;
    op op_;
  };

  struct read {
    explicit read(access_scheduler& ctrl, op_type_t const type) : ctrl_{ctrl} {
      ctrl_.start_read(type);
    }
    ~read() { ctrl_.end_read(); }
    access_scheduler& ctrl_;
  };

  struct write {
    explicit write(access_scheduler& ctrl, op_type_t const type) : ctrl_{ctrl} {
      ctrl_.start_write(type);
    }
    ~write() { ctrl_.end_write(); }
    access_scheduler& ctrl_;
  };

  void start_read(op_type_t const type) {
    std::unique_lock<std::mutex> l{lock_, std::defer_lock_t{}};

  aquire:  // while loop not suitable because condition needs to be locked, to
    l.lock();
    if (write_active_) {
      auto const op = current_op<Data>();
      read_queue_.emplace_back(type, op->shared_from_this());
      l.unlock();
      op->suspend(/*finish = */ false);
      goto aquire;
    }

    ++read_count_;
  }

  void end_read() {
    std::unique_lock<std::mutex> l{lock_};
    --read_count_;
    if (read_count_ == 0 && !write_queue_.empty()) {
      unqueue(write_queue_);
    } else if (!read_queue_.empty()) {
      unqueue(read_queue_);
    }
  }

  void start_write(op_type_t const type) {
    std::unique_lock<std::mutex> l{lock_, std::defer_lock_t{}};

  aquire:  // while loop not suitable because condition needs to be locked, too
    l.lock();
    if (write_active_ || read_count_ != 0) {
      auto const op = current_op<Data>();
      read_queue_.emplace_back(type, op->shared_from_this());
      l.unlock();
      op->suspend(/*finish = */ false);
      goto aquire;
    }

    write_active_ = true;
  }

  void end_write() {
    std::unique_lock<std::mutex> l{lock_};
    write_active_ = false;
    if (!write_queue_.empty()) {
      unqueue(write_queue_);
    } else if (!read_queue_.empty()) {
      unqueue(read_queue_);
    }
  }

  void unqueue(std::vector<queue_entry>& queue) {
    auto& entry = queue.front();
    entry.type_ == op_type_t::IO ? this->enqueue_io(entry.op_)
                                 : this->enqueue_work(entry.op_);
    queue.erase(begin(queue));
  }

  template <typename Fn>
  void enqueue(Data d, Fn&& fn, op_id id, op_type_t op_type, access_t access) {
    switch (access) {
      case access_t::NONE:
        (op_type == op_type_t::IO)
            ? this->enqueue_io(d, std::forward<Fn>(fn), id)
            : this->enqueue_work(d, std::forward<Fn>(fn), id);
        break;

      case access_t::READ: {
        auto f = [fn, op_type, this]() {
          write r{*this, op_type};
          return fn();
        };
        (op_type == op_type_t::IO) ? this->enqueue_io(d, std::move(f), id)
                                   : this->enqueue_work(d, std::move(f), id);
        break;
      }

      case access_t::WRITE: {
        auto f = [fn, op_type, this]() {
          read r{*this, op_type};
          return fn();
        };
        (op_type == op_type_t::IO) ? this->enqueue_io(d, std::move(f), id)
                                   : this->enqueue_work(d, std::move(f), id);
        break;
      }
    }
  }

  template <typename Fn>
  void enqueue_read_io(Data d, Fn&& fn, op_id id) {
    this->enqueue_io(d,
                     [fn, this]() {
                       read r{*this, op_type_t::IO};
                       return fn();
                     },
                     id);
  }

  template <typename Fn>
  void enqueue_write_io(Data d, Fn&& fn, op_id id) {
    this->enqueue_io(d,
                     [fn, this]() {
                       write r{*this, op_type_t::IO};
                       return fn();
                     },
                     id);
  }

  template <typename Fn>
  auto post_read_io(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_io(d,
                                    [fn, this]() {
                                      read r{*this, op_type_t::IO};
                                      return fn();
                                    },
                                    id);
  }

  template <typename Fn>
  auto post_void_read_io(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_io(d,
                                    [fn, this]() {
                                      read r{*this, op_type_t::IO};
                                      return fn();
                                    },
                                    id);
  }

  template <typename Fn>
  auto post_write_io(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_io(d,
                                    [fn, this]() {
                                      write r{*this, op_type_t::IO};
                                      return fn();
                                    },
                                    id);
  }

  template <typename Fn>
  auto post_void_write_io(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_io(d,
                                    [fn, this]() {
                                      write r{*this, op_type_t::IO};
                                      return fn();
                                    },
                                    id);
  }

  template <typename Fn>
  void enqueue_read_work(Data d, Fn&& fn, op_id id) {
    this->enqueue_work(d,
                       [fn, this]() {
                         read r{*this, op_type_t::WORK};
                         return fn();
                       },
                       id);
  }

  template <typename Fn>
  void enqueue_write_work(Data d, Fn&& fn, op_id id) {
    this->enqueue_work(d,
                       [fn, this]() {
                         write r{*this, op_type_t::WORK};
                         return fn();
                       },
                       id);
  }

  template <typename Fn>
  auto post_read_work(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_work(d,
                                      [fn, this]() {
                                        read r{*this, op_type_t::WORK};
                                        return fn();
                                      },
                                      id);
  }

  template <typename Fn>
  auto post_void_read_work(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_work(d,
                                      [fn, this]() {
                                        read r{*this, op_type_t::WORK};
                                        return fn();
                                      },
                                      id);
  }

  template <typename Fn>
  auto post_write_work(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_work(d,
                                      [fn, this]() {
                                        write r{*this, op_type_t::WORK};
                                        return fn();
                                      },
                                      id);
  }

  template <typename Fn>
  auto post_void_write_work(Data d, Fn&& fn, op_id id) {
    return scheduler<Data>::post_work(d,
                                      [fn, this]() {
                                        write r{*this, op_type_t::WORK};
                                        return fn();
                                      },
                                      id);
  }

  std::mutex lock_;
  std::vector<queue_entry> write_queue_;
  std::vector<queue_entry> read_queue_;
  size_t read_count_ = 0;
  bool write_active_ = false;
};

}  // namespace ctx