#pragma once

#include "cista/containers/hash_map.h"

#include "ctx/access_t.h"
#include "ctx/res_id_t.h"

namespace ctx {

struct access_data {
  cista::raw::hash_map<res_id_t, access_t> res_access_;
};

}  // namespace ctx