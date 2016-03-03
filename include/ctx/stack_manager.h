#pragma once

#include <vector>
#include <mutex>

#include "ctx_config.h"
#ifdef CTX_ENABLE_VALGRIND
#include "valgrind/valgrind.h"
#endif

namespace ctx {

constexpr auto kStackSize = 8 * 1024;

struct stack_handle {
  stack_handle() : mem(nullptr) {}

  void* mem;

#ifdef CTX_ENABLE_VALGRIND
  unsigned id;
#endif
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
    stack_handle s;
    s.mem = static_cast<char*>(allocate(kStackSize)) + kStackSize;

#ifdef CTX_ENABLE_VALGRIND
    auto from = s.mem;
    auto to = static_cast<char*>(from) - kStackSize;
    s.id = VALGRIND_STACK_REGISTER(from, to);
#endif

    return s;
  }

  void dealloc(stack_handle const& s) {
    std::free(static_cast<char*>(s.mem) - kStackSize);

#ifdef CTX_ENABLE_VALGRIND
    VALGRIND_STACK_DEREGISTER(s.id);
#endif
  }
};

}  // namespace ctx
