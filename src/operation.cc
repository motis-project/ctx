#include "ctx/operation.h"

namespace ctx {

thread_local operation* operation::this_op;

}  // namespace ctx