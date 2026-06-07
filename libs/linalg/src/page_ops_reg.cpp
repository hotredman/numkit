// libs/linalg/src/page_ops_reg.cpp
//
// Register half of the paged-matrix builtins: the CallContext wrappers
// pageinv / pageeig / pagesvd / pagepinv / pagenorm / pagemldivide /
// pagemrdivide / pagelsqminnorm that delegate to the engine-free compute in
// page_ops.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/page_ops.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace numkit::linalg {
namespace detail {

void pageinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("pageinv: requires exactly 1 argument",
                    0, 0, "pageinv", "", "numkit:pageinv:nargin");
    outs[0] = pageinv(args[0], ctx.engine->resource());
}

void pageeig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("pageeig: requires exactly 1 argument",
                    0, 0, "pageeig", "", "numkit:pageeig:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [V, D] = pageeig_VD(args[0], mr);
        outs[0] = std::move(V);
        outs[1] = std::move(D);
    } else {
        outs[0] = pageeig_values(args[0], mr);
    }
}

void pagesvd_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("pagesvd: requires exactly 1 argument",
                    0, 0, "pagesvd", "", "numkit:pagesvd:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [U, S, V] = pagesvd_decompose(args[0], mr);
        outs[0] = std::move(U);
        outs[1] = std::move(S);
        if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(V);
    } else {
        outs[0] = pagesvd_values(args[0], mr);
    }
}

void pagepinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("pagepinv: requires (A) or (A, tol)",
                    0, 0, "pagepinv", "", "numkit:pagepinv:nargin");
    const double tol = (args.size() == 2) ? args[1].toScalar() : -1.0;
    outs[0] = pagepinv(args[0], tol, ctx.engine->resource());
}

void pagenorm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("pagenorm: requires (A) or (A, p)",
                    0, 0, "pagenorm", "", "numkit:pagenorm:nargin");
    double p = 2.0;
    if (args.size() == 2) {
        if (args[1].isChar() || args[1].isString()) {
            const std::string s = args[1].toString();
            if      (s == "fro" || s == "Fro") p = -2.0; // sentinel
            else if (s == "inf" || s == "Inf") p = std::numeric_limits<double>::infinity();
            else throw Error("pagenorm: string p must be 'fro' or 'inf'",
                             0, 0, "pagenorm", "", "numkit:pagenorm:badStringP");
        } else {
            p = args[1].toScalar();
        }
    }
    auto *mr = ctx.engine->resource();
    if (p == -2.0) {
        // Frobenius per page. Apply pageShape + iterate norm_fro.
        const int nd = args[0].dims().ndim();
        if (nd < 2 || nd > 3)
            throw Error("pagenorm: input must be 2-D or 3-D",
                        0, 0, "pagenorm", "", "numkit:pagenorm:badDim");
        const std::size_t m = static_cast<std::size_t>(args[0].dims().dim(0));
        const std::size_t n = static_cast<std::size_t>(args[0].dims().dim(1));
        const std::size_t pages = (nd == 2) ? 1
            : static_cast<std::size_t>(args[0].dims().dim(2));
        Value out = (nd == 2) ? Value::matrix(1, 1, ValueType::DOUBLE, mr)
                              : Value::matrix3d(1, 1, pages, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (std::size_t pp = 0; pp < pages; ++pp) {
            const double *src = args[0].doubleData() + pp * m * n;
            double s = 0.0;
            for (std::size_t k = 0; k < m * n; ++k) s += src[k] * src[k];
            od[pp] = std::sqrt(s);
        }
        outs[0] = std::move(out);
        return;
    }
    outs[0] = pagenorm(args[0], p, mr);
}

void pagemldivide_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("pagemldivide: requires (A, B)",
                    0, 0, "pagemldivide", "", "numkit:pagemldivide:nargin");
    outs[0] = pagemldivide(args[0], args[1], ctx.engine->resource());
}

void pagemrdivide_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("pagemrdivide: requires (A, B)",
                    0, 0, "pagemrdivide", "", "numkit:pagemrdivide:nargin");
    outs[0] = pagemrdivide(args[0], args[1], ctx.engine->resource());
}

void pagelsqminnorm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("pagelsqminnorm: requires (A, B[, tol])",
                    0, 0, "pagelsqminnorm", "", "numkit:pagelsqminnorm:nargin");
    const bool have_tol = (args.size() == 3);
    const double tol = have_tol ? args[2].toScalar() : 0.0;
    outs[0] = pagelsqminnorm(args[0], args[1], have_tol, tol, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::linalg
