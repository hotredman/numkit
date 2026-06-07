// libs/linalg/src/properties_reg.cpp
//
// Register half of the matrix-property builtins: the CallContext wrappers
// inv / trace / det / rank / cond / normest / rcond / condest / condeig
// that delegate to the engine-free compute in properties.cpp. condeig's
// multi-output form reuses the engine-free eig_symmetric / eig_general_VD
// (hence the eig.hpp include). library.cpp forward-declares + registers
// these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/properties.hpp>
#include <numkit/linalg/eig.hpp>     // eig_symmetric / eig_general_VD (condeig [V,D])

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace numkit::linalg {
namespace detail {

void inv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("inv: requires exactly 1 argument",
                    0, 0, "inv", "", "numkit:inv:nargin");
    outs[0] = inv(args[0], ctx.engine->resource());
}

void trace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("trace: requires exactly 1 argument",
                    0, 0, "trace", "", "numkit:trace:nargin");
    outs[0] = trace(args[0], ctx.engine->resource());
}

void det_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("det: requires exactly 1 argument",
                    0, 0, "det", "", "numkit:det:nargin");
    outs[0] = det(args[0], ctx.engine->resource());
}

void rank_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("rank: requires (A) or (A, tol)",
                    0, 0, "rank", "", "numkit:rank:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = rank_of(args[0], tol, ctx.engine->resource());
}

void cond_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("cond: requires (A) or (A, p)",
                    0, 0, "cond", "", "numkit:cond:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = cond_2norm(args[0], mr);
        return;
    }
    const Value &p = args[1];
    int p_kind = 2;
    if (p.isChar() || p.isString()) {
        const std::string s = p.toString();
        if      (s == "fro" || s == "Fro") p_kind = 4;
        else if (s == "inf" || s == "Inf") p_kind = 3;
        else throw Error("cond: string p must be 'fro' or 'inf'",
                         0, 0, "cond", "", "numkit:cond:badStringP");
    } else {
        const double pv = p.toScalar();
        if      (pv == 1.0) p_kind = 1;
        else if (pv == 2.0) p_kind = 2;
        else if (std::isinf(pv)) p_kind = 3;
        else throw Error("cond: numeric p must be 1, 2, or Inf",
                         0, 0, "cond", "", "numkit:cond:badP");
    }
    outs[0] = cond_pnorm(args[0], p_kind, mr);
}

void normest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("normest: requires exactly 1 argument",
                    0, 0, "normest", "", "numkit:normest:nargin");
    outs[0] = normest(args[0], ctx.engine->resource());
}

void rcond_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rcond: requires (A)",
                    0, 0, "rcond", "", "numkit:rcond:nargin");
    outs[0] = rcond(args[0], ctx.engine->resource());
}

void condest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("condest: requires (A)",
                    0, 0, "condest", "", "numkit:condest:nargin");
    outs[0] = condest(args[0], ctx.engine->resource());
}

// condeig has three calling forms:
//   s            = condeig(A)
//   [V, D, s]    = condeig(A)        — V, D from eig(A), s as above.
// The 2-output `[V, D] = condeig(A)` is documented in MATLAB only as
// the same as `eig(A)`. Dispatch matches.
void condeig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("condeig: requires (A)",
                    0, 0, "condeig", "", "numkit:condeig:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout <= 1) {
        outs[0] = condeig(args[0], mr);
        return;
    }
    // [V, D[, s]] form: hand off to eig for the first two, compute s
    // separately (no shared work yet — refactor candidate when this
    // becomes a hot path).
    const Value &A = args[0];
    // Eig dispatch matches the eig_reg path.
    auto sym = [&]() -> bool {
        if (A.dims().ndim() != 2) return false;
        const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
        if (n != static_cast<std::size_t>(A.dims().dim(1))) return false;
        const double *p = A.doubleData();
        const double tol = 1e-10;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) {
                const double d = std::fabs(p[i + j * n] - p[j + i * n]);
                const double s = std::max(std::fabs(p[i + j * n]),
                                           std::fabs(p[j + i * n]));
                if (d > tol * (1.0 + s)) return false;
            }
        return true;
    }();
    if (sym) {
        auto [V, D] = eig_symmetric(A, mr);
        outs[0] = std::move(V);
        outs[1] = std::move(D);
    } else {
        auto [V, D] = eig_general_VD(A, mr);
        outs[0] = std::move(V);
        outs[1] = std::move(D);
    }
    if (nargout >= 3 && outs.size() >= 3)
        outs[2] = condeig(A, mr);
}

} // namespace detail
} // namespace numkit::linalg
