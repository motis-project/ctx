#pragma once

#include "ctx/channel.h"

#include "ctx/operation.h"

namespace ctx {

struct worker {
  using input_channel = channel<std::shared_ptr<operation>>;

  worker(input_channel& in) : in_(in) {}

  void run() {
    input_channel::queue_element work;
    while (in_[in_.any] >> work) {
      work.value->resume();
    }
  }

  input_channel& in_;
};

}  // namespace ctx