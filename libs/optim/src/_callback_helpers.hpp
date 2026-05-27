// libs/optim/src/_callback_helpers.hpp
//
// Thin Value↔double wrappers around a numkit::FnHandle MATLAB-style
// callback. Used by libs/optim's scalar / vector iterative solvers
// (fzero, fminbnd, fminsearch) so the math kernels can stay in
// native double-precision while the public API remains a generic
// `void(Span<const Value>, Span<Value>)` callback.

#pragma once

#include <memory_resource>
#include <numkit/core/fn_handle.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

namespace numkit::optim::detail::callback {

/// @brief Invoke `fn(x)` with a scalar argument and return the scalar
/// result.
///
/// @param fn  MATLAB-style callback.
/// @param x   Scalar evaluation point.
/// @param mr  Memory resource passed through to the callback for any
///            Value construction it does.
/// @return    `fn(x)` as a double.
/// @throws Error  If the callback returns a non-scalar value.
inline double evalScalar(FnHandle fn, double x,
                         std::pmr::memory_resource *mr)
{
    Value arg = Value::scalar(x, mr);
    Value out;
    Span<const Value> args(&arg, 1);
    Span<Value>       outs(&out, 1);
    fn(args, outs, mr);
    if (!out.isScalar() && out.numel() != 1)
        throw Error("callback: handle must return a scalar value",
                     0, 0, "callback", "", "numkit:callback:nonScalar");
    return out.elemAsDouble(0);
}

/// @brief Invoke `fn(x)` with a vector argument and return the scalar
/// result.
///
/// Packs `x[0..n)` into a 1×n DOUBLE row vector before the call.
///
/// @param fn  MATLAB-style callback.
/// @param x   Vector evaluation point (length n).
/// @param n   Vector length.
/// @param mr  Memory resource used for the intermediate Value and
///            passed through to the callback.
/// @return    `fn(x)` as a double.
inline double evalVecToScalar(FnHandle fn, const double *x, std::size_t n,
                              std::pmr::memory_resource *mr)
{
    Value v = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n) {
        double *d = v.doubleDataMut();
        for (std::size_t i = 0; i < n; ++i) d[i] = x[i];
    }
    Value out;
    Value args[1] = { std::move(v) };
    fn(Span<const Value>(args, 1), Span<Value>(&out, 1), mr);
    return out.toScalar();
}

} // namespace numkit::optim::detail::callback
