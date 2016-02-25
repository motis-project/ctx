#pragma once

#include <vector>

namespace ctx {

constexpr auto kStackSize = 64 * 1024;

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
      dealloc(stack);
    }
  }

  void* alloc() {
    auto stack = free_stacks_.empty() ? allocate(kStackSize) : get_free_stack();
    return static_cast<char*>(stack) + kStackSize;
  }

  void dealloc(void* stack) {
    if (stack == nullptr) {
      return;
    }
    void* limit = static_cast<char*>(stack) - kStackSize;
    free_stacks_.push_back(limit);
  }

private:
  void* get_free_stack() {
    auto stack = free_stacks_.back();
    free_stacks_.resize(free_stacks_.size() - 1);
    return stack;
  }

  std::vector<void*> free_stacks_;
};

}  // namespace ctx