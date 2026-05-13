// libs/builtin/src/math/_callback_helpers.hpp
//
// Thin Value↔double wrapper around a numkit::FnHandle MATLAB-style
// callback. Used by libs/builtin's iterative numerical kernels
// (integral, …) so the math stays in native double-precision while
// the public API keeps its generic callback signature.

#pragma once

#include <memory_resource>
#include <numkit/core/fn_handle.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin::detail::callback {

/// @brief Invoke `fn(x)` with a scalar argument and return the scalar
/// result.
///
/// @param fn  MATLAB-style callback.
/// @param x   Scalar evaluation point.
/// @return    `fn(x)` as a double.
/// @throws Error  If the callback returns a non-scalar value.
inline double evalScalar(FnHandle fn, double x)
{
    Value arg = Value::scalar(x);
    Value out;
    Span<const Value> args(&arg, 1);
    Span<Value>       outs(&out, 1);
    fn(args, outs);
    if (!out.isScalar() && out.numel() != 1)
        throw Error("callback: handle must return a scalar value",
                     0, 0, "callback", "", "m:callback:nonScalar");
    return out.elemAsDouble(0);
}

} // namespace numkit::builtin::detail::callback
