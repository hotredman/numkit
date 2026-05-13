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
//       [](auto args, auto outs) {
//           double x = args[0].toScalar();
//           outs[0] = Value::scalar(x*x - 2.0);
//       },
//       Value::matrix(...), mr);
//
// Engine-adapter side (libs/optim/src/local/fzero.cpp):
//   void fzero_reg(args, nargout, outs, ctx) {
//       auto handle = args[0];
//       auto cb = [&ctx, &handle](Span<const Value> a, Span<Value> o) {
//           ctx.engine->callFunctionHandle(handle, a, o);
//       };
//       outs[0] = fzero(cb, args[1], ctx.engine->resource());
//   }

#pragma once

#include <numkit/core/function_ref.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit {

/// @brief MATLAB-style function-handle callback.
///
/// `args` carries the per-call inputs; `outs` is the caller-allocated
/// output buffer (its `.size()` is the requested `nargout`). The
/// callback fills `outs[0..outs.size())`.
using FnHandle = function_ref<void(Span<const Value> args,
                                    Span<Value>       outs)>;

} // namespace numkit
