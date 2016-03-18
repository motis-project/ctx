#pragma once

#include "ctx/scheduler.h"

#include "ctx/operation.h"

namespace ctx {

template <typename Data>
struct operation;

template <typename Data>
scheduler<Data>::scheduler()
    : next_id_(0) {}

template <typename Data>
template <typename Fn>
auto scheduler<Data>::post(Data data, Fn fn, op_id id) {
  using return_t = decltype(fn());
  auto f = std::make_shared<future<Data, return_t>>();
  enqueue(std::forward<Data>(data), std::function<void()>([f, fn]() {
            std::exception_ptr ex;
            try {
              f->set(fn());
              return;
            } catch (...) {
              ex = std::current_exception();
            }
            f->set(ex);
          }),
          std::move(id));
  return f;
}

template <typename Data>
template <typename Fn>
auto scheduler<Data>::post_void(Data data, Fn fn, op_id id) {
  auto f = std::make_shared<future<Data, void>>();
  enqueue(std::forward<Data>(data), std::function<void()>([f, fn]() {
            std::exception_ptr ex;
            try {
              fn();
              f->set();
              return;
            } catch (...) {
              ex = std::current_exception();
            }
            f->set(ex);
          }),
          std::move(id));
  return f;
}

template <typename Data>
void scheduler<Data>::enqueue(Data data, std::function<void()> fn, op_id id) {
  id.index = ++next_id_;
  enqueue(std::make_shared<operation<Data>>(
      std::forward<Data>(data), std::move(fn), *this, std::move(id)));
  return id;
}

template <typename Data>
void scheduler<Data>::enqueue(std::shared_ptr<operation<Data>> const& op) {
  ios_.post([op]() { op->resume(); });
}

}  // namespace ctx
