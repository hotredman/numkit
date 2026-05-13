// core/include/numkit/core/fn_handle.hpp
//
// numkit::FnHandle — the canonical MATLAB-style callback type used by
// the numerical-library API for any function that takes a
// user-supplied function handle (fzero, integral, fminsearch,
// cellfun, structfun, …).
//
// Signature mirrors what MATLAB function handles are at the engine
// boundary: variadic Value inputs, caller-allocated Value outputs.
// `outs.size()` plays the role of MATLAB's `nargout`.
//
// Library-side example:
//   Value fzero(FnHandle fn, const Value &x0, mr);
//
// User-side (C++):
//   auto root = fzero(
//       [](auto args, auto outs, auto *mr) {
//           double x = args[0].toScalar();
//           outs[0] = Value::scalar(x*x - 2.0, mr);  // honour caller mr
//       },
//       Value::matrix(...), mr);
//
// Engine-adapter side (libs/optim/src/local/fzero.cpp):
//   void fzero_reg(args, nargout, outs, ctx) {
//       auto handle = args[0];
//       auto cb = [&ctx, &handle](Span<const Value> a, Span<Value> o,
//                                 std::pmr::memory_resource * /*mr*/) {
//           // Engine allocates results; library caller absorbs them
//           // via move-assign into o, so mr is unused on this side.
//           auto r = ctx.engine->callFunctionHandleMulti(handle, a, o.size());
//           for (size_t i = 0; i < o.size() && i < r.size(); ++i)
//               o[i] = std::move(r[i]);
//       };
//       outs[0] = fzero(cb, args[1], ctx.engine->resource());
//   }

#pragma once

#include <memory_resource>

#include <numkit/core/function_ref.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit {

/// @brief MATLAB-style function-handle callback.
///
/// `args` carries the per-call inputs; `outs` is the caller-allocated
/// output buffer (its `.size()` is the requested `nargout`). `mr` is
/// the caller's memory resource — the callback should honour it when
/// constructing the output Values so PMR allocation chains through.
/// The callback fills `outs[0..outs.size())`.
using FnHandle = function_ref<void(Span<const Value>           args,
                                    Span<Value>                 outs,
                                    std::pmr::memory_resource * mr)>;

} // namespace numkit
