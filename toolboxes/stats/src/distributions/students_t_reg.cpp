// toolboxes/signal/src/distributions/students_t_reg.cpp
//
// CallContext register half of distributions/students_t.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "students_t_detail.hpp"
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

void tpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tpdf: requires (x, nu)", 0, 0, "tpdf", "", "numkit:tpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &nu = args[1];
    if (nu.isScalar())
        outs[0] = tpdf(args[0], nu.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], nu, mr, "tpdf", tpdfK);
}

void tcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("tcdf: requires (x, nu[, 'upper'])", 0, 0, "tcdf", "", "numkit:tcdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &nu = a[1];
    Value v;
    if (nu.isScalar()) {
        v = tcdf(a[0], nu.toScalar(), mr);
    } else {
        // F(x; nu) = 1 - ½·I_z(nu/2, ½) for x≥0 (½·I_z for x<0), z=nu/(nu+x²);
        // nu→∞ → normcdf. betainc broadcasts (z, nu/2, ½); sign + Inf per element.
        const Value &x = a[0];
        const size_t nx = x.numel(), nnu = nu.numel();
        if (nx == 0 || nnu == 0) {
            v = dist_empty_like(nx == 0 ? x : nu, mr);
        } else {
            const size_t N = dist_match_numel({nx, nnu}, "tcdf");
            Value z = broadcast_dist2(x, nu, mr, "tcdf", [](double xi, double nui) -> double {
                if (!(nui > 0.0)) return std::numeric_limits<double>::quiet_NaN();
                if (std::isinf(nui)) return 1.0;   // placeholder (overridden below)
                return nui / (nui + xi * xi);
            });
            Value hnu = elementwise(nu, [](double ni) { return std::isinf(ni) ? 1.0 : 0.5 * ni; }, mr);
            Value Iz = ::numkit::math::betainc(z, hnu, Value::scalar(0.5, mr), mr);
            const Value &ref = (nnu == N) ? nu : x;
            v = dist_empty_like(ref, mr);
            double *od = v.doubleDataMut();
            const size_t nIz = Iz.numel();
            const double NaN = std::numeric_limits<double>::quiet_NaN();
            for (size_t i = 0; i < N; ++i) {
                const double nui = nu.elemAsDouble(nnu == 1 ? 0 : i);
                const double xi = x.elemAsDouble(nx == 1 ? 0 : i);
                if (!(nui > 0.0)) { od[i] = NaN; continue; }
                if (std::isinf(nui)) { od[i] = tNormCdf(xi); continue; }
                const double Ii = Iz.elemAsDouble(nIz == 1 ? 0 : i);
                od[i] = (xi >= 0.0) ? 1.0 - 0.5 * Ii : 0.5 * Ii;
            }
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void tinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tinv: requires (p, nu)", 0, 0, "tinv", "", "numkit:tinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &nu = args[1];
    if (nu.isScalar()) {
        outs[0] = tinv(args[0], nu.toScalar(), mr);
        return;
    }
    // z = betaincinv(2·min(p,1-p), nu/2, ½); x = sign·sqrt(nu(1/z - 1));
    // nu→∞ → norminv(p). Per-element fixup mirrors the scalar tinv exactly.
    const Value &p = args[0];
    const size_t np = p.numel(), nnu = nu.numel();
    if (np == 0 || nnu == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : nu, mr);
        return;
    }
    const size_t N = dist_match_numel({np, nnu}, "tinv");
    Value qv = elementwise(p, [](double pi) {
        if (std::isnan(pi) || pi < 0.0 || pi > 1.0) return 0.5;
        return (pi >= 0.5) ? 2.0 * (1.0 - pi) : 2.0 * pi;
    }, mr);
    Value hnu = elementwise(nu, [](double ni) { return std::isinf(ni) ? 1.0 : 0.5 * ni; }, mr);
    Value zv = ::numkit::math::betaincinv(qv, hnu, Value::scalar(0.5, mr), mr);
    const Value &ref = (nnu == N) ? nu : p;
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const size_t nzv = zv.numel();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double PINF = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < N; ++i) {
        const double pi = p.elemAsDouble(np == 1 ? 0 : i);
        const double nui = nu.elemAsDouble(nnu == 1 ? 0 : i);
        if (!(nui > 0.0) || std::isnan(pi) || pi < 0.0 || pi > 1.0) { od[i] = NaN; continue; }
        if (pi == 0.0) { od[i] = -PINF; continue; }
        if (pi == 1.0) { od[i] = PINF; continue; }
        if (std::isinf(nui)) { od[i] = tNormInv(pi); continue; }
        const double zi = zv.elemAsDouble(nzv == 1 ? 0 : i);
        if (zi <= 0.0) { od[i] = (pi >= 0.5) ? PINF : -PINF; continue; }
        const double mag = std::sqrt(nui * (1.0 / zi - 1.0));
        od[i] = (pi >= 0.5) ? mag : -mag;
    }
    outs[0] = std::move(out);
}

void trnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("trnd: requires nu[, sz...]", 0, 0, "trnd", "", "numkit:trnd:nargin");
    const double nu = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = trnd(nu, rows, cols, ctx.engine->resource());
}

void tstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx.engine->resource(), "tstat",
                       [](double nu) { return tstat(nu); });
}

void nctpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nctpdf: requires (x, nu, delta)",
                    0, 0, "nctpdf", "", "numkit:nctpdf:nargin");
    outs[0] = nctpdf(args[0], args[1].toScalar(), args[2].toScalar(),
                     ctx.engine->resource());
}

void nctcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const size_t n = stripUpperFlag(args, upper);
    if (n < 3)
        throw Error("nctcdf: requires (x, nu, delta[, 'upper'])",
                    0, 0, "nctcdf", "", "numkit:nctcdf:nargin");
    outs[0] = nctcdf(args[0], args[1].toScalar(), args[2].toScalar(), upper,
                     ctx.engine->resource());
}

void nctinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nctinv: requires (p, nu, delta)",
                    0, 0, "nctinv", "", "numkit:nctinv:nargin");
    outs[0] = nctinv(args[0], args[1].toScalar(), args[2].toScalar(),
                     ctx.engine->resource());
}

void nctstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nctstat: requires (nu, delta)",
                    0, 0, "nctstat", "", "numkit:nctstat:nargin");
    auto [m, v] = nctstat(args[0].toScalar(), args[1].toScalar());
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout >= 2)
        outs[1] = Value::scalar(v, ctx.engine->resource());
}

void nctrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nctrnd: requires (nu, delta[, sz...])",
                    0, 0, "nctrnd", "", "numkit:nctrnd:nargin");
    const double nu = args[0].toScalar();
    const double delta = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = nctrnd(nu, delta, rows, cols, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
