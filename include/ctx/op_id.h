#pragma once

namespace ctx {

struct op_id {
  op_id() = default;
  op_id(char const* name, char const* created_at, int parent_index)
      : name(name),
        created_at(created_at),
        parent_index(parent_index),
        index(0) {}

  friend bool operator<(op_id const& lhs, op_id const& rhs) {
    return lhs.index < rhs.index;
  }

  friend bool operator==(op_id const& lhs, op_id const& rhs) {
    return lhs.index == rhs.index;
  }

  char const* name;
  char const* created_at;
  unsigned parent_index;
  unsigned index;
};

}  // namespace ctx
