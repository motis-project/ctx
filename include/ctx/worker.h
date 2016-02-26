#pragma once

#include "ctx/channel.h"

#include "ctx/operation.h"

namespace ctx {

struct worker {
  using input_channel = channel<operation*>;

  worker(input_channel& in) : in_(in) {}

  void run() {
    input_channel::queue_element work;
    while (in_[in_.any] >> work) {
      work.value->resume();

      // TODO move memory management to scheduler
      if (work.value->finished_) {
        delete work.value;
      }
    }
  }

  input_channel& in_;
};

}  // namespace ctx