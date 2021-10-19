#pragma once

#include <vector>

#include "ctx/access_t.h"
#include "ctx/res_id_t.h"

namespace ctx {

struct access_request {
  res_id_t res_id_{0U};
  access_t access_{access_t::NONE};
};

using accesses_t = std::vector<ctx::access_request>;

}  // namespace ctx