#pragma once

#include "ctx/scheduler.h"

#include "ctx/operation.h"

namespace ctx {

template <typename Data>
struct operation;

template <typename Data>
template <typename Fn>
auto scheduler<Data>::post(Data data, Fn fn, op_id id) {
  id.index = ++next_id_;
  auto f = std::make_shared<future<Data, decltype(fn())>>(id);
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
  id.index = ++next_id_;
  auto f = std::make_shared<future<Data, void>>(id);
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
  enqueue(std::make_shared<operation<Data>>(
      std::forward<Data>(data), std::move(fn), *this, std::move(id)));
}

template <typename Data>
void scheduler<Data>::enqueue(std::shared_ptr<operation<Data>> const& op) {
  op->on_transition(transition::ENQUEUE);
  ios_.post([op]() { op->resume(); });
}

}  // namespace ctx
