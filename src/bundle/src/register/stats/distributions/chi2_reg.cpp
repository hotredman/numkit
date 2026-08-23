// toolboxes/signal/src/distributions/chi2_reg.cpp
//
// CallContext register half of distributions/chi2.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/datafun.hpp>  // RngContext + rand/randn/randi/randperm (session-scoped, no global/mutex)
#include <numkit/builtin/specfun.hpp> // gammainc, gammaincinv
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include "distributions/chi2_detail.hpp"
#include "distributions/dist_helpers.hpp"
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

void chi2pdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("chi2pdf: requires (x, k)", 0, 0, "chi2pdf", "", "numkit:chi2pdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &k = args[1];
    // Scalar k: hoisted fast path (unchanged). Vector k: broadcast.
    if (k.isScalar())
        outs[0] = chi2pdf(args[0], k.toScalar(), mr);
    else
        outs[0] = broadcast_dist2(args[0], k, mr, "chi2pdf", chi2pdfK);
}

void chi2cdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 2)
        throw Error("chi2cdf: requires (x, k[, 'upper'])", 0, 0, "chi2cdf", "", "numkit:chi2cdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &k = a[1];
    Value v;
    if (k.isScalar()) {
        v = chi2cdf(a[0], k.toScalar(), mr);
    } else {
        // F(x; k) = gammainc(max(0,x/2), k/2); gammainc broadcasts (xs, k/2)
        // and gammaincScalar gives NaN where k/2<=0 (matches k<=0 → NaN).
        Value xs = elementwise(a[0], [](double xi) { return std::max(0.0, 0.5 * xi); }, mr);
        Value half_k = elementwise(k, [](double ki) { return 0.5 * ki; }, mr);
        v = ::numkit::builtin::gammainc(xs, half_k, mr);
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void chi2inv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("chi2inv: requires (p, k)", 0, 0, "chi2inv", "", "numkit:chi2inv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[0];
    const Value &k = args[1];
    if (k.isScalar()) {
        outs[0] = chi2inv(p, k.toScalar(), mr);   // unchanged fast path
        return;
    }
    // Broadcast: x = 2·gammaincinv(p, k/2); k<0 → NaN; k==0 → 0 (p∈[0,1]) / NaN.
    const size_t np = p.numel(), nk = k.numel();
    if (np == 0 || nk == 0) {
        outs[0] = dist_empty_like(np == 0 ? p : k, mr);
        return;
    }
    const size_t N = dist_match_numel({np, nk}, "chi2inv");
    Value half_k = elementwise(k, [](double ki) { return 0.5 * ki; }, mr);
    Value q = ::numkit::builtin::gammaincinv(p, half_k, mr);
    const size_t nq = q.numel();
    const Value &ref = (nk == N) ? k : p;
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double pi = p.elemAsDouble(np == 1 ? 0 : i);
        const double ki = k.elemAsDouble(nk == 1 ? 0 : i);
        const double qi = q.elemAsDouble(nq == 1 ? 0 : i);
        od[i] = (ki < 0.0) ? NaN
                           : (ki == 0.0 ? ((pi >= 0.0 && pi <= 1.0) ? 0.0 : NaN) : 2.0 * qi);
    }
    outs[0] = std::move(out);
}

void chi2rnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chi2rnd: requires k[, sz...]", 0, 0, "chi2rnd", "", "numkit:chi2rnd:nargin");
    const double k = args[0].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 1, rows, cols);
    outs[0] = chi2rnd(ctx.engine->rng(), k, rows, cols, ctx.engine->resource());
}

void chi2stat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_1arg(args, nargout, outs, ctx.engine->resource(), "chi2stat",
                       [](double k) { return chi2stat(k); });
}

} // namespace detail

} // namespace numkit::stats
