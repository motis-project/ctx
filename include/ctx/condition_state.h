#pragma once

#include <mutex>

namespace ctx {

struct condition_state {
  std::mutex mutex;
  bool suspended;
};

}  // namespace ctx