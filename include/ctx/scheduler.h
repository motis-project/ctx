#pragma once

#include <memory>

#include "ctx/channel.h"
#include "ctx/stack_manager.h"

namespace ctx {

struct operation;

struct scheduler {
  channel<std::shared_ptr<operation>> queue_;
  stack_manager stack_manager_;
};

}  // namespace ctx