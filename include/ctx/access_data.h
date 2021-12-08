#pragma once

#include <map>

#include "ctx/access_t.h"
#include "ctx/res_id_t.h"

namespace ctx {

struct access_data {
  std::map<res_id_t, access_t> res_access_;
};

}  // namespace ctx
