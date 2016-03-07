#pragma once

namespace ctx {

struct op_id {
  op_id() = default;
  op_id(char const* name, char const* created_at)
      : name(name), created_at(created_at), index(0) {}

  friend bool operator<(op_id const& lhs, op_id const& rhs) {
    return lhs.index < rhs.index;
  }

  friend bool operator==(op_id const& lhs, op_id const& rhs) {
    return lhs.index == rhs.index;
  }

  char const* name;
  char const* created_at;
  unsigned index;
};

}  // namespace ctx
