// libs/signal/src/distributions/negbin_reg.cpp
//
// CallContext register half of distributions/negbin.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/stats/distributions/negbin.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "negbin_detail.hpp"
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

void nbinpdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nbinpdf: requires (k, r, p)", 0, 0, "nbinpdf", "", "numkit:nbinpdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &r = args[1];
    const Value &p = args[2];
    if (r.isScalar() && p.isScalar())
        outs[0] = nbinpdf(args[0], r.toScalar(), p.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], r, p, mr, "nbinpdf", nbinpdfK);
}

void nbincdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 3)
        throw Error("nbincdf: requires (k, r, p[, 'upper'])", 0, 0, "nbincdf", "", "numkit:nbincdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &r = a[1];
    const Value &p = a[2];
    Value v;
    if (r.isScalar() && p.isScalar()) {
        v = nbincdf(a[0], r.toScalar(), p.toScalar(), mr);
    } else {
        // Per-element F(k_i; r_i, p_i) via nbin_cdf_scalar (betainc); invalid →
        // NaN. Same per-element-betainc cost as the scalar nbincdf over a vec k.
        const Value &k = a[0];
        const size_t nk = k.numel(), nr = r.numel(), np = p.numel();
        if (nk == 0 || nr == 0 || np == 0) {
            v = dist_empty_like(nk == 0 ? k : (nr == 0 ? r : p), mr);
        } else {
            const size_t N = dist_match_numel({nk, nr, np}, "nbincdf");
            const Value &ref = (nr == N) ? r : (np == N ? p : k);
            v = dist_empty_like(ref, mr);
            double *od = v.doubleDataMut();
            const double NaN = std::numeric_limits<double>::quiet_NaN();
            for (size_t i = 0; i < N; ++i) {
                const double ri = r.elemAsDouble(nr == 1 ? 0 : i);
                const double pi = p.elemAsDouble(np == 1 ? 0 : i);
                const double ki = k.elemAsDouble(nk == 1 ? 0 : i);
                od[i] = nbin_params_ok(ri, pi) ? nbin_cdf_scalar(ki, ri, pi, mr) : NaN;
            }
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void nbininv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nbininv: requires (q, r, p)", 0, 0, "nbininv", "", "numkit:nbininv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &r = args[1];
    const Value &p = args[2];
    if (r.isScalar() && p.isScalar()) {
        outs[0] = nbininv(args[0], r.toScalar(), p.toScalar(), mr);
        return;
    }
    const Value &q = args[0];
    const size_t nq = q.numel(), nr = r.numel(), np = p.numel();
    if (nq == 0 || nr == 0 || np == 0) {
        outs[0] = dist_empty_like(nq == 0 ? q : (nr == 0 ? r : p), mr);
        return;
    }
    const size_t N = dist_match_numel({nq, nr, np}, "nbininv");
    const Value &ref = (nr == N) ? r : (np == N ? p : q);
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double ri = r.elemAsDouble(nr == 1 ? 0 : i);
        const double pi = p.elemAsDouble(np == 1 ? 0 : i);
        const double qi = q.elemAsDouble(nq == 1 ? 0 : i);
        od[i] = nbin_params_ok(ri, pi) ? nbin_inv_scalar(qi, ri, pi) : NaN;
    }
    outs[0] = std::move(out);
}

void nbinrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nbinrnd: requires (r, p[, m, n])", 0, 0, "nbinrnd", "", "numkit:nbinrnd:nargin");
    const double r = args[0].toScalar();
    const double p = args[1].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) rows = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4 && !args[3].isEmpty()) cols = static_cast<size_t>(args[3].toScalar());
    else if (args.size() >= 3) cols = rows;
    outs[0] = nbinrnd(r, p, rows, cols, ctx.engine->resource());
}

void nbinstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx.engine->resource(), "nbinstat",
                       [](double r, double p) { return nbinstat(r, p); });
}

} // namespace detail

} // namespace numkit::stats
