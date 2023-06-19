#pragma once

#include <cstdlib>
#include <mutex>
#include <vector>

#include "ctx_config.h"
#ifdef CTX_ENABLE_VALGRIND
#include "valgrind/valgrind.h"
#endif

namespace ctx {

constexpr auto kStackSize = 512 * 1024;

struct stack_handle {
  stack_handle();
  stack_handle(void* allocated_mem);

  void* get_allocated_mem();
  void set_allocated_mem(void* mem);
  void* get_stack();
  void* get_stack_end();

#ifdef CTX_ENABLE_VALGRIND
  unsigned id;
#endif

private:
  void* stack;
};

void* allocate(std::size_t const num_bytes);

struct stack_manager {
  ~stack_manager();

  stack_handle alloc();
  void dealloc(stack_handle&);

  struct node {
    inline void* take();
    inline void push(void* p);
    node* next_{nullptr};
  } list_{};
  std::mutex list_mutex_;
};

}  // namespace ctx
