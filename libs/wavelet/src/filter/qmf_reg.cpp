// libs/wavelet/src/filter/qmf_reg.cpp
//
// Register half of the filter helpers wrev / qmf: the CallContext builtins
// that delegate to the engine-free compute in qmf.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/filter/qmf.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

namespace numkit::wavelet {
namespace detail {

// y = wrev(x): reverse along the first non-singleton dimension. MATLAB
// behaviour:
//   row vector    -> reverse element order (= flip).
//   col vector    -> reverse element order.
//   matrix (M×N)  -> reverse each column independently (= flipud).
//   complex input -> preserve complex type.
//
// Bug fix 2026-05-08: previous impl treated the input as a flat
// numel-element vector and reversed in column-major order. For matrices
// that gave a full reversal (rows AND cols flipped) instead of MATLAB's
// per-column reverse. Also dropped imaginary parts on complex input
// (used elemAsDouble + doubleDataMut).
void wrev_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wrev: requires one input vector",
                    0, 0, "wrev", "", "numkit:wrev:nargin");
    outs[0] = wrev(args[0], ctx.engine->resource());
}

// y = qmf(x[, p]): quadrature mirror filter.
//   y(k) = (-1)^(k-1)        · x(N-k+1)   if p == 0 (default)
//   y(k) = (-1)^k = -(-1)^(k-1) · x(N-k+1)   if p == 1
//
// Verified vs MATLAB R2025b:
//   qmf([1 2 3 4])    → [4 -3 2 -1]
//   qmf([1 2 3 4], 1) → [-4 3 -2 1]
void qmf_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("qmf: requires one input vector",
                    0, 0, "qmf", "", "numkit:qmf:nargin");
    int p = 0;
    if (args.size() >= 2) p = static_cast<int>(args[1].toScalar());
    outs[0] = qmf(args[0], p, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
