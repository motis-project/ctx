#pragma once

#include "ctx/condition_variable.h"

namespace ctx {

template <typename Data>
std::weak_ptr<operation<Data>> get_caller() {
  auto op = reinterpret_cast<operation<Data>*>(this_op);
  if (op) {
    return op->shared_from_this();
  } else {
    return std::weak_ptr<operation<Data>>();
  }
}

template <typename Data>
condition_variable<Data>::condition_variable()
    : caller_(get_caller<Data>()) {}

template <typename Data>
template <typename Predicate>
void condition_variable<Data>::wait(Predicate pred) {
  while (!pred()) {
    wait();
  }
}

template <typename Data>
void condition_variable<Data>::wait() {
  auto caller_lock = caller_.lock();
  assert(caller_lock);
  caller_lock->suspend(/* finished = */ false);
}

template <typename Data>
void condition_variable<Data>::notify() {
  auto caller_lock = caller_.lock();
  if (caller_lock) {
    caller_lock->sched_.enqueue_work(caller_lock);
  }
}

}  // namespace ctx
