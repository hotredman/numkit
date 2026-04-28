// libs/dsp/src/MDspFilter.cpp

#include <numkit/signal/digital_filtering/filter.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <vector>

namespace numkit::signal {

namespace {

// Direct Form II transposed core, applied to a flat input buffer.
// Used by both filter() and filtfilt()'s forward/backward passes.
std::vector<double> applyFilterDf2t(const double *bn, size_t nb,
                                    const double *an, size_t na,
                                    const double *input, size_t len)
{
    const size_t nfilt = std::max(nb, na);
    std::vector<double> out(len);
    std::vector<double> z(nfilt, 0.0);
    for (size_t n = 0; n < len; ++n) {
        out[n] = (nb > 0 ? bn[0] : 0.0) * input[n] + z[0];
        for (size_t i = 1; i < nfilt; ++i) {
            z[i - 1] = (i < nb ? bn[i] : 0.0) * input[n]
                       - (i < na ? an[i] : 0.0) * out[n]
                       + (i < nfilt - 1 ? z[i] : 0.0);
        }
    }
    return out;
}

} // namespace

// ── filter ────────────────────────────────────────────────────────────
Value filter(Allocator &alloc, const Value &b, const Value &a, const Value &x)
{
    const size_t nb = b.numel(), na = a.numel(), nx = x.numel();
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const double *xd = x.doubleData();

    const double a0 = ad[0];
    if (a0 == 0.0)
        throw Error("filter: a(1) must be nonzero",
                     0, 0, "filter", "", "m:filter:zeroLead");

    std::vector<double> bn(nb), an(na);
    for (size_t i = 0; i < nb; ++i)
        bn[i] = bd[i] / a0;
    for (size_t i = 0; i < na; ++i)
        an[i] = ad[i] / a0;

    auto out = applyFilterDf2t(bn.data(), nb, an.data(), na, xd, nx);

    auto r = createLike(x, ValueType::DOUBLE, &alloc);
    double *y = r.doubleDataMut();
    for (size_t n = 0; n < nx; ++n)
        y[n] = out[n];
    return r;
}

// ── filtfilt ──────────────────────────────────────────────────────────
Value filtfilt(Allocator &alloc, const Value &b, const Value &a, const Value &x)
{
    const size_t nb = b.numel(), na = a.numel(), nx = x.numel();
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const double *xd = x.doubleData();

    const double a0 = ad[0];
    if (a0 == 0.0)
        throw Error("filtfilt: a(1) must be nonzero",
                     0, 0, "filtfilt", "", "m:filtfilt:zeroLead");

    std::vector<double> bn(nb), an(na);
    for (size_t i = 0; i < nb; ++i)
        bn[i] = bd[i] / a0;
    for (size_t i = 0; i < na; ++i)
        an[i] = ad[i] / a0;

    const size_t nfilt = std::max(nb, na);

    // Edge-reflect padding, length 3 * nfilt on each side (capped at nx - 1)
    size_t nEdge = 3 * nfilt;
    if (nEdge >= nx)
        nEdge = nx - 1;

    const size_t extLen = nx + 2 * nEdge;
    std::vector<double> ext(extLen);
    for (size_t i = 0; i < nEdge; ++i)
        ext[i] = 2.0 * xd[0] - xd[nEdge - i];
    for (size_t i = 0; i < nx; ++i)
        ext[nEdge + i] = xd[i];
    for (size_t i = 0; i < nEdge; ++i)
        ext[nEdge + nx + i] = 2.0 * xd[nx - 1] - xd[nx - 2 - i];

    auto fwd = applyFilterDf2t(bn.data(), nb, an.data(), na, ext.data(), extLen);
    std::reverse(fwd.begin(), fwd.end());
    auto bwd = applyFilterDf2t(bn.data(), nb, an.data(), na, fwd.data(), fwd.size());
    std::reverse(bwd.begin(), bwd.end());

    auto r = createLike(x, ValueType::DOUBLE, &alloc);
    double *y = r.doubleDataMut();
    for (size_t i = 0; i < nx; ++i)
        y[i] = bwd[nEdge + i];
    return r;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void filter_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("filter: requires 3 arguments",
                     0, 0, "filter", "", "m:filter:nargin");
    outs[0] = filter(ctx.engine->allocator(), args[0], args[1], args[2]);
}

void filtfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("filtfilt: requires 3 arguments",
                     0, 0, "filtfilt", "", "m:filtfilt:nargin");
    outs[0] = filtfilt(ctx.engine->allocator(), args[0], args[1], args[2]);
}

} // namespace detail

} // namespace numkit::signal
