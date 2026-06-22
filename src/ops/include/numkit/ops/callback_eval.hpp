// ops/callback_eval.hpp
//
// Thin Value<->double wrappers around a numkit::FnHandle MATLAB-style callback,
// so the ops iterative solver kernels (root_solve: brent/findBracket/brentMin/
// nelderMead) can run in native double precision while the public objective
// stays a generic `void(Span<const Value>, Span<Value>)` handle. Core-free
// (value layer only); FnHandle is value/fn_handle.hpp (L0).

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/value/error.hpp>
#include <numkit/value/fn_handle.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>
#include <utility>

namespace numkit::ops {

/// Invoke `fn(x)` with a scalar argument; return the scalar result.
/// Throws if the callback returns a non-scalar value.
inline double evalScalar(FnHandle fn, double x, std::pmr::memory_resource *mr)
{
    Value             arg = Value::scalar(x, mr);
    Value             out;
    Span<const Value> args(&arg, 1);
    Span<Value>       outs(&out, 1);
    fn(args, outs, mr);
    if (!out.isScalar() && out.numel() != 1)
        throw Error("callback: handle must return a scalar value", 0, 0, "callback", "",
                    "numkit:callback:nonScalar");
    return out.elemAsDouble(0);
}

/// Invoke `fn(x)` with a vector argument (x[0..n) packed into a 1xn DOUBLE row);
/// return the scalar result.
inline double evalVecToScalar(FnHandle fn, const double *x, std::size_t n,
                              std::pmr::memory_resource *mr)
{
    Value v = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n) {
        double *d = v.doubleDataMut();
        for (std::size_t i = 0; i < n; ++i) d[i] = x[i];
    }
    Value out;
    Value args[1] = {std::move(v)};
    fn(Span<const Value>(args, 1), Span<Value>(&out, 1), mr);
    return out.toScalar();
}

} // namespace numkit::ops
