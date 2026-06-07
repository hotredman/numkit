// libs/linalg/src/norms_reg.cpp
//
// Register half of the norm builtins: the CallContext wrappers norm /
// vecnorm that parse the p / dim arguments and delegate to the engine-free
// compute in norms.cpp. library.cpp forward-declares + registers these by
// name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/norms.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cmath>

namespace numkit::linalg {
namespace detail {

void norm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("norm: requires (X) or (X, p)",
                    0, 0, "norm", "", "numkit:norm:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = norm_value(args[0], 2.0, mr);
        return;
    }
    const Value &p = args[1];
    if (p.isChar() || p.isString()) {
        const auto s = p.toString();
        if (s == "fro" || s == "Fro") {
            outs[0] = norm_fro(args[0], mr);
            return;
        }
        if (s == "inf" || s == "Inf") {
            outs[0] = norm_inf(args[0], mr);
            return;
        }
        throw Error("norm: string p must be 'fro' or 'inf'",
                    0, 0, "norm", "", "numkit:norm:badStringP");
    }
    const double pv = p.toScalar();
    if (std::isinf(pv)) {
        if (pv > 0.0) {
            outs[0] = norm_inf(args[0], mr);
            return;
        }
        // p = -Inf is a VECTOR norm only: min(|v|). MATLAB rejects it for
        // matrices ("the only matrix norms are 1, 2, Inf, 'fro'"). Previously
        // both +Inf and -Inf fell through to norm_inf (max) — wrong for -Inf.
        const Value &X = args[0];
        const bool isVec = !X.dims().is3D()
                           && (X.dims().rows() <= 1 || X.dims().cols() <= 1);
        if (!isVec)
            throw Error("norm: the only matrix norms available are 1, 2, "
                        "Inf, and 'fro'",
                        0, 0, "norm", "", "numkit:norm:badMatrixP");
        outs[0] = vecnorm(X, pv, 0, mr);  // vecnorm handles -Inf = min(|v|)
        return;
    }
    outs[0] = norm_value(args[0], pv, mr);
}

void vecnorm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("vecnorm: requires (A [, p [, dim]])",
                    0, 0, "vecnorm", "", "numkit:vecnorm:nargin");
    double p = 2.0;
    int dim = 0;
    if (args.size() >= 2) p = args[1].toScalar();
    if (args.size() >= 3) dim = static_cast<int>(args[2].toScalar());
    outs[0] = vecnorm(args[0], p, dim, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::linalg
