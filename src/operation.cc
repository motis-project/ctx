#include "ctx/operation.h"

namespace ctx {

__thread operation* operation::this_op;

}  // namespace ctx