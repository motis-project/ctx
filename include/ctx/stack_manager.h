#pragma once

#include <vector>
#include <mutex>

#include "ctx_config.h"
#ifdef CTX_ENABLE_VALGRIND
#include "valgrind/valgrind.h"
#endif

namespace ctx {

constexpr auto kStackSize = 128 * 1024;

struct stack_handle {
  stack_handle() : stack(nullptr) {}
  stack_handle(void* allocated_mem) { set_allocated_mem(allocated_mem); }

  inline void* get_allocated_mem() {
    return stack == nullptr ? nullptr : static_cast<char*>(stack) - kStackSize;
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
    stack_handle s(allocate(kStackSize));

#ifdef CTX_ENABLE_VALGRIND
    s.id = VALGRIND_STACK_REGISTER(s.get_stack(), s.get_stack_end());
#endif

    return s;
  }

  void dealloc(stack_handle& s) {
    std::free(s.get_allocated_mem());
    s.set_allocated_mem(nullptr);

#ifdef CTX_ENABLE_VALGRIND
    VALGRIND_STACK_DEREGISTER(s.id);
#endif
  }
};

}  // namespace ctx
