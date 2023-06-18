#include "ctx/stack_manager.h"

#include <cassert>

namespace ctx {

stack_handle::stack_handle() : stack(nullptr) {}

stack_handle::stack_handle(void* allocated_mem) {
  set_allocated_mem(allocated_mem);
}

void* stack_handle::get_allocated_mem() { return get_stack_end(); }

void stack_handle::set_allocated_mem(void* mem) {
  stack = mem == nullptr ? nullptr : static_cast<char*>(mem) + kStackSize;
}

void* stack_handle::get_stack() { return stack; }

void* stack_handle::get_stack_end() {
  return stack == nullptr ? nullptr : static_cast<char*>(stack) - kStackSize;
}

void* allocate(std::size_t const num_bytes) {
  auto mem = std::malloc(num_bytes);
  if (!mem) {
    throw std::bad_alloc();
  }
  return mem;
}

stack_manager::~stack_manager() {
  while (list_.next_ != nullptr) {
    std::free(list_.take());
  }
}

stack_handle stack_manager::alloc() {
  {
    auto const lock = std::lock_guard{list_mutex_};
    if (list_.next_ != nullptr) {
      stack_handle s(list_.take());
#ifdef CTX_ENABLE_VALGRIND
      s.id = VALGRIND_STACK_REGISTER(s.get_stack(), s.get_stack_end());
#endif
      return s;
    }
  }

  stack_handle s(allocate(kStackSize));
#ifdef CTX_ENABLE_VALGRIND
  s.id = VALGRIND_STACK_REGISTER(s.get_stack(), s.get_stack_end());
#endif
  return s;
}

void stack_manager::dealloc(stack_handle& s) {
  auto const lock = std::lock_guard{list_mutex_};
  list_.push(s.get_allocated_mem());

#ifdef CTX_ENABLE_VALGRIND
  VALGRIND_STACK_DEREGISTER(s.id);
#endif
}

void* stack_manager::node::take() {
  assert(next_ != nullptr);
  auto const ptr = next_;
  next_ = next_->next_;
  return ptr;
}

void stack_manager::node::push(void* p) {
  auto const mem_ptr = reinterpret_cast<node*>(p);
  mem_ptr->next_ = next_;
  next_ = mem_ptr;
}

}  // namespace ctx