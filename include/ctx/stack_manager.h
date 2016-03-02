#pragma once

#include <vector>
#include <mutex>

// #define ENABLE_VALGRIND

#ifdef ENABLE_VALGRIND
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
  ~stack_manager() {
    for (auto stack : free_stacks_) {
      free(stack);
    }
    free_stacks_.clear();
  }

  stack_handle alloc() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto stack = free_stacks_.empty() ? allocate(kStackSize) : get_free_stack();

#ifdef ENABLE_VALGRIND
    auto id = VALGRIND_STACK_REGISTER(static_cast<char*>(stack) + kStackSize, stack);
#else
    auto id = 42;
#endif

    return {static_cast<char*>(stack) + kStackSize, id};
  }

  void dealloc(stack_handle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.mem == nullptr) {
      return;
    }
    void* limit = static_cast<char*>(handle.mem) - kStackSize;
    free_stacks_.push_back(limit);

#ifdef ENABLE_VALGRIND
    VALGRIND_STACK_DEREGISTER(handle.id);
#endif
  }

private:
  void* get_free_stack() {
    auto stack = free_stacks_.back();
    free_stacks_.resize(free_stacks_.size() - 1);
    return stack;
  }

  std::vector<void*> free_stacks_;
  std::mutex mutex_;
};

}  // namespace ctx