#pragma once

#define CTX_STRING1(x) #x
#define CTX_STRING2(x) CTX_STRING1(x)
#define CTX_LOCATION __FILE__ ":" CTX_STRING2(__LINE__)

#define ctx_call(data, fn) \
  ctx::call<decltype(data)>(data, fn, ctx::op_id("unknown", CTX_LOCATION))

namespace ctx {

template <typename Data, typename Fn>
auto call(Data data, Fn fn, op_id id)
    -> std::shared_ptr<future<Data, decltype(fn())>> {
  return reinterpret_cast<operation<Data>*>(this_op)->sched_.post(
      std::forward<Data>(data), std::forward<Fn>(fn), std::move(id));
}

}  // namespace ctx
