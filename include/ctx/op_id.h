#pragma once

#include <string>

namespace ctx {

struct op_id {
  op_id() = default;
  op_id(char const* created_at)
      : name(created_at), created_at(created_at), parent_index(0), index(0) {}
  op_id(std::string name, char const* created_at, int parent_index)
      : name(std::move(name)),
        created_at(created_at),
        parent_index(parent_index),
        index(0) {}

  friend bool operator<(op_id const& lhs, op_id const& rhs) {
    return lhs.index < rhs.index;
  }

  friend bool operator==(op_id const& lhs, op_id const& rhs) {
    return lhs.index == rhs.index;
  }

  std::string name;
  char const* created_at;
  unsigned parent_index{0};
  unsigned index{0};
};

}  // namespace ctx
