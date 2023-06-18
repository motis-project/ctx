#pragma once

#include <cstdlib>
#include <mutex>
#include <vector>

#include "ctx_config.h"
#ifdef CTX_ENABLE_VALGRIND
#include "valgrind/valgrind.h"
#endif

namespace ctx {

constexpr auto kStackSize = 1024 * 1024;

struct stack_handle {
  stack_handle() : stack(nullptr) {}
  stack_handle(void* allocated_mem) { set_allocated_mem(allocated_mem); }

  inline void* get_allocated_mem() {
    return get_stack_end();
  }

  inline void set_allocated_mem(void* mem) {
    stack = mem == nullptr ? nullptr : static_cast<char*>(mem) + kStackSize;
  }

  inline void* get_stack() { return stack; }

  inline void* get_stack_end() {
    return stack == nullptr ? nullptr : static_cast<char*>(stack) - kStackSize;
  }

#ifdef CTX_ENABLE_VALGRIND
  unsigned id;
#endif

private:
  void* stack;
};

inline void* allocate(std::size_t const num_bytes) {
  auto mem = std::malloc(num_bytes);
  if (!mem) {
    throw std::bad_alloc();
  }
  return mem;
}

struct stack_manager {
  stack_handle alloc() {
    {
      auto const lock = std::lock_guard{list_mutex_};
      if (list_.next_ != nullptr) {
        return stack_handle{list_.take()};
      }
    }

    stack_handle s(allocate(kStackSize));

#ifdef CTX_ENABLE_VALGRIND
    s.id = VALGRIND_STACK_REGISTER(s.get_stack(), s.get_stack_end());
#endif

    return s;
  }

  void dealloc(stack_handle& s) {
    auto const lock = std::lock_guard{list_mutex_};
    list_.push(s.get_allocated_mem());

#ifdef CTX_ENABLE_VALGRIND
    VALGRIND_STACK_DEREGISTER(s.id);
#endif
  }

  struct node {
    inline void* take() {
      assert(next_ != nullptr);
      auto const ptr = next_;
      next_ = next_->next_;
      return ptr;
    }
    inline void push(void* p) {
      auto const mem_ptr = reinterpret_cast<node*>(p);
      mem_ptr->next_ = next_;
      next_ = mem_ptr;
    }
    node* next_{nullptr};
  } list_{};
  std::mutex list_mutex_;
};

}  // namespace ctx
