// libs/signal/src/distributions/fisher_f_reg.cpp
//
// CallContext register half of distributions/fisher_f.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "fisher_f_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void fpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("fpdf: requires (x, v1, v2)", 0, 0, "fpdf", "", "numkit:fpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &v1 = args[1];
    const Value &v2 = args[2];
    if (v1.isScalar() && v2.isScalar())
        outs[0] = fpdf(args[0], v1.toScalar(), v2.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], v1, v2, mr, "fpdf", fpdfK);
}

void fcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a0 = args.subspan(0, stripUpperFlag(args, upper));
    if (a0.size() < 3)
        throw Error("fcdf: requires (x, v1, v2[, 'upper'])", 0, 0, "fcdf", "", "numkit:fcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &v1 = a0[1];
    const Value &v2 = a0[2];
    Value v;
    if (v1.isScalar() && v2.isScalar()) {
        v = fcdf(a0[0], v1.toScalar(), v2.toScalar(), mr);
    } else {
        // F(x; v1, v2) = I_y(v1/2, v2/2), y = v1*x/(v1*x+v2). y broadcasts
        // (x,v1,v2) (v<=0 → NaN, x<=0 → 0); betainc broadcasts (y, v1/2, v2/2).
        const Value &x = a0[0];
        const size_t nx = x.numel(), n1 = v1.numel(), n2 = v2.numel();
        if (nx == 0 || n1 == 0 || n2 == 0) {
            v = dist_empty_like(nx == 0 ? x : (n1 == 0 ? v1 : v2), mr);
        } else {
            dist_match_numel({nx, n1, n2}, "fcdf");
            Value y = broadcast_dist3(x, v1, v2, mr, "fcdf", [](double xi, double d1, double d2) -> double {
                if (d1 <= 0.0 || d2 <= 0.0) return std::numeric_limits<double>::quiet_NaN();
                if (xi <= 0.0) return 0.0;
                return (d1 * xi) / (d1 * xi + d2);
            });
            Value a = elementwise(v1, [](double d) { return 0.5 * d; }, mr);
            Value b = elementwise(v2, [](double d) { return 0.5 * d; }, mr);
            v = ::numkit::builtin::betainc(y, a, b, mr);
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void finv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("finv: requires (p, v1, v2)", 0, 0, "finv", "", "numkit:finv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &v1 = args[1];
    const Value &v2 = args[2];
    if (v1.isScalar() && v2.isScalar()) {
        outs[0] = finv(args[0], v1.toScalar(), v2.toScalar(), mr);
        return;
    }
    // z = betaincinv(p, v1/2, v2/2); x = (v2/v1)·z/(1-z). Mirrors finv exactly
    // (z<=0 → 0, z>=1 → Inf, NaN z propagates); v<=0 → NaN per element.
    const Value &p = args[0];
    const size_t np = p.numel(), n1 = v1.numel(), n2 = v2.numel();
    if (np == 0 || n1 == 0 || n2 == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : (n1 == 0 ? v1 : v2), mr);
        return;
    }
    const size_t N = dist_match_numel({np, n1, n2}, "finv");
    Value a = elementwise(v1, [](double d) { return 0.5 * d; }, mr);
    Value b = elementwise(v2, [](double d) { return 0.5 * d; }, mr);
    Value z = ::numkit::builtin::betaincinv(p, a, b, mr);
    const Value &ref = (n1 == N) ? v1 : (n2 == N ? v2 : p);
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const size_t nz = z.numel();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double d1 = v1.elemAsDouble(n1 == 1 ? 0 : i);
        const double d2 = v2.elemAsDouble(n2 == 1 ? 0 : i);
        if (d1 <= 0.0 || d2 <= 0.0) { od[i] = NaN; continue; }
        const double zi = z.elemAsDouble(nz == 1 ? 0 : i);
        if (zi <= 0.0) { od[i] = 0.0; continue; }
        if (zi >= 1.0) { od[i] = std::numeric_limits<double>::infinity(); continue; }
        od[i] = (d2 / d1) * zi / (1.0 - zi);
    }
    outs[0] = std::move(out);
}

void frnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("frnd: requires (v1, v2[, sz...])", 0, 0, "frnd", "", "numkit:frnd:nargin");
    const double v1 = args[0].toScalar();
    const double v2 = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = frnd(v1, v2, rows, cols, ctx.engine->resource());
}

void fstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "fstat",
                       [](double v1, double v2) { return fstat(v1, v2); });
}

void ncfpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ncfpdf: requires (x, nu1, nu2, delta)",
                    0, 0, "ncfpdf", "", "numkit:ncfpdf:nargin");
    outs[0] = ncfpdf(args[0], args[1].toScalar(), args[2].toScalar(),
                     args[3].toScalar(), ctx.engine->resource());
}

void ncfcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 4)
        throw Error("ncfcdf: requires (x, nu1, nu2, delta[, 'upper'])",
                    0, 0, "ncfcdf", "", "numkit:ncfcdf:nargin");
    outs[0] = ncfcdf(args[0], args[1].toScalar(), args[2].toScalar(),
                     args[3].toScalar(), upper, ctx.engine->resource());
}

void ncfinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ncfinv: requires (p, nu1, nu2, delta)",
                    0, 0, "ncfinv", "", "numkit:ncfinv:nargin");
    outs[0] = ncfinv(args[0], args[1].toScalar(), args[2].toScalar(),
                     args[3].toScalar(), ctx.engine->resource());
}

void ncfstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncfstat: requires (nu1, nu2, delta)",
                    0, 0, "ncfstat", "", "numkit:ncfstat:nargin");
    auto [m, v] = ncfstat(args[0].toScalar(), args[1].toScalar(), args[2].toScalar());
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout >= 2)
        outs[1] = Value::scalar(v, ctx.engine->resource());
}

void ncfrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ncfrnd: requires (nu1, nu2, delta[, sz...])",
                    0, 0, "ncfrnd", "", "numkit:ncfrnd:nargin");
    const double nu1 = args[0].toScalar();
    const double nu2 = args[1].toScalar();
    const double delta = args[2].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 3, rows, cols);
    outs[0] = ncfrnd(nu1, nu2, delta, rows, cols, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
