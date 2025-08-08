#pragma once

#include <string>

namespace ctx {

struct op_id {
  op_id() = default;
  op_id(char const* created_at)
      : name_{created_at},
        created_at_{created_at},
        parent_index_{0},
        index_{0} {}
  op_id(std::string name, char const* created_at, unsigned parent_index)
      : name_{std::move(name)},
        created_at_{created_at},
        parent_index_{parent_index},
        index_{0} {}

  friend bool operator<(op_id const& lhs, op_id const& rhs) {
    return lhs.index_ < rhs.index_;
  }

  friend bool operator==(op_id const& lhs, op_id const& rhs) {
    return lhs.index_ == rhs.index_;
  }

  std::string name_;
  char const* created_at_;
  unsigned parent_index_{0};
  unsigned index_{0};
};

}  // namespace ctx
