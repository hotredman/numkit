// toolboxes/signal/src/distributions/poisson_reg.cpp
//
// CallContext register half of distributions/poisson.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>
#include <numkit/stats/distributions/poisson.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "poisson_detail.hpp"
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

void poisspdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("poisspdf: requires (k, lambda)", 0, 0, "poisspdf", "", "numkit:poisspdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &lam = args[1];
    if (lam.isScalar())
        outs[0] = poisspdf(args[0], lam.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], lam, mr, "poisspdf", poisspdfK);
}

void poisscdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("poisscdf: requires (k, lambda[, 'upper'])", 0, 0, "poisscdf", "", "numkit:poisscdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &lam = a[1];
    Value v;
    if (lam.isScalar()) {
        v = poisscdf(a[0], lam.toScalar(), mr);
    } else {
        // F(k; λ) = Q(⌊k⌋+1, λ) = 1 - gammainc(λ, ⌊k⌋+1). gammainc broadcasts
        // (λ, ⌊k⌋+1); per-element fixup for k<0 / λ<=0.
        const Value &k = a[0];
        const size_t nk = k.numel(), nl = lam.numel();
        if (nk == 0 || nl == 0) {
            v = dist_empty_like(nk == 0 ? k : lam, mr);
        } else {
            const size_t N = dist_match_numel({nk, nl}, "poisscdf");
            Value xs = elementwise(k, [](double ki) { return ki < 0.0 ? 1.0 : std::floor(ki) + 1.0; }, mr);
            Value lower = ::numkit::builtin::gammainc(lam, xs, mr);   // P(⌊k⌋+1, λ)
            const Value &ref = (nl == N) ? lam : k;
            v = dist_empty_like(ref, mr);
            double *od = v.doubleDataMut();
            const size_t nlo = lower.numel();
            const double NaN = std::numeric_limits<double>::quiet_NaN();
            for (size_t i = 0; i < N; ++i) {
                const double li = lam.elemAsDouble(nl == 1 ? 0 : i);
                const double ki = k.elemAsDouble(nk == 1 ? 0 : i);
                if (li < 0.0) { od[i] = NaN; continue; }
                if (li == 0.0) { od[i] = (ki >= 0.0) ? 1.0 : 0.0; continue; }
                if (ki < 0.0) { od[i] = 0.0; continue; }
                od[i] = 1.0 - lower.elemAsDouble(nlo == 1 ? 0 : i);
            }
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void poissinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("poissinv: requires (p, lambda)", 0, 0, "poissinv", "", "numkit:poissinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &lam = args[1];
    if (lam.isScalar()) {
        outs[0] = poissinv(args[0], lam.toScalar(), mr);
        return;
    }
    const Value &p = args[0];
    const size_t np = p.numel(), nl = lam.numel();
    if (np == 0 || nl == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : lam, mr);
        return;
    }
    const size_t N = dist_match_numel({np, nl}, "poissinv");
    const Value &ref = (nl == N) ? lam : p;
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double li = lam.elemAsDouble(nl == 1 ? 0 : i);
        const double pi = p.elemAsDouble(np == 1 ? 0 : i);
        od[i] = (li < 0.0) ? NaN : poiss_inv_scalar(pi, li);
    }
    outs[0] = std::move(out);
}

void poissrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poissrnd: requires lambda[, sz...]", 0, 0, "poissrnd", "", "numkit:poissrnd:nargin");
    const double lambda = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = poissrnd(lambda, rows, cols, ctx.engine->resource());
}

void poisstat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx.engine->resource(), "poisstat",
                       [](double lambda) { return poisstat(lambda); });
}

} // namespace detail

} // namespace numkit::stats
