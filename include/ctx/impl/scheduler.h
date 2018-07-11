#pragma once

#include "ctx/scheduler.h"

#include "ctx/operation.h"

namespace ctx {

template <typename Data>
struct operation;

template <typename Data>
template <typename Fn>
auto scheduler<Data>::post_io(Data d, Fn fn, op_id id) {
  id.index = ++next_id_;
  auto f = std::make_shared<future<Data, decltype(fn())>>(id);
  enqueue_io(std::forward<Data>(d), std::function<void()>([fn, f]() {
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
future_ptr<Data, void> scheduler<Data>::post_void_io(Data d, Fn fn, op_id id) {
  id.index = ++next_id_;
  auto f = std::make_shared<future<Data, void>>(id);
  enqueue_io(std::forward<Data>(d), std::function<void()>([fn, f]() {
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
template <typename Fn>
auto scheduler<Data>::post_work(Data d, Fn fn, op_id id) {
  id.index = ++next_id_;
  auto f = std::make_shared<future<Data, decltype(fn())>>(id);
  enqueue_work(std::forward<Data>(d), std::function<void()>([fn, f]() {
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
future_ptr<Data, void> scheduler<Data>::post_void_work(Data d, Fn fn,
                                                       op_id id) {
  id.index = ++next_id_;
  auto f = std::make_shared<future<Data, void>>(id);
  enqueue_work(std::forward<Data>(d), std::function<void()>([fn, f]() {
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
void scheduler<Data>::enqueue_io(Data d, std::function<void()> fn, op_id id) {
  id.index = ++next_id_;
  enqueue_io(std::make_shared<operation<Data>>(
      std::forward<Data>(d), std::move(fn), *this, std::move(id)));
}

template <typename Data>
void scheduler<Data>::enqueue_io(std::shared_ptr<operation<Data>> const& op) {
  op->on_transition(transition::ENQUEUE);
  ios_.post_io_task([op]() { op->resume(); });
}

template <typename Data>
void scheduler<Data>::enqueue_work(Data d, std::function<void()> fn, op_id id) {
  id.index = ++next_id_;
  enqueue_work(std::make_shared<operation<Data>>(
      std::forward<Data>(d), std::move(fn), *this, std::move(id)));
}

template <typename Data>
void scheduler<Data>::enqueue_work(std::shared_ptr<operation<Data>> const& op) {
  op->on_transition(transition::ENQUEUE);
  ios_.post_work_task([op]() { op->resume(); });
}

}  // namespace ctx
