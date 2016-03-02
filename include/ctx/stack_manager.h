#pragma once

#include <vector>
#include <mutex>

#include "ctx_config.h"
#ifdef CTX_ENABLE_VALGRIND
#include "valgrind/valgrind.h"
#endif

namespace ctx {

constexpr auto kStackSize = 64 * 1024;

struct stack_handle {
  void* mem;
  unsigned int id;
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
    auto id = 0u;
    auto stack = static_cast<char*>(allocate(kStackSize));
#ifdef CTX_ENABLE_VALGRIND
    id = VALGRIND_STACK_REGISTER(stack, static_cast<char*>(stack) + kStackSize);
#endif
    return {stack + kStackSize, id};
  }
  void dealloc(stack_handle const& handle) {
    std::free(static_cast<char*>(handle.mem) - kStackSize);

#ifdef CTX_ENABLE_VALGRIND
    VALGRIND_STACK_DEREGISTER(handle.id);
#endif
  }
};

}  // namespace ctx