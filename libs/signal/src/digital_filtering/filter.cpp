// libs/signal/src/digital_filtering/filter.cpp

#include <numkit/signal/digital_filtering/filter.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cstring>
#include <memory_resource>

namespace numkit::signal {

namespace {

// Direct Form II transposed core, applied to a flat input buffer.
// Used by both filter() and filtfilt()'s forward/backward passes.
// Optional `zi` (length ziLen) seeds the initial delay state; when
// `zfOut` is non-null the final state (length nfilt-1) is written there —
// this implements MATLAB's filter(b,a,x,zi) and [y,zf] = filter(...).
ScratchVec<double> applyFilterDf2t(const double *bn, size_t nb, const double *an, size_t na, const double *input, size_t len, std::pmr::memory_resource *mr, const double *zi = nullptr, size_t ziLen = 0, double *zfOut = nullptr)
{
    const size_t nfilt = std::max(nb, na);
    ScratchVec<double> out(len, mr);
    ScratchVec<double> z(nfilt, mr);
    for (size_t i = 0; i < nfilt; ++i)
        z[i] = (zi && i < ziLen) ? zi[i] : 0.0;
    for (size_t n = 0; n < len; ++n) {
        out[n] = (nb > 0 ? bn[0] : 0.0) * input[n] + z[0];
        for (size_t i = 1; i < nfilt; ++i) {
            z[i - 1] = (i < nb ? bn[i] : 0.0) * input[n]
                       - (i < na ? an[i] : 0.0) * out[n]
                       + (i < nfilt - 1 ? z[i] : 0.0);
        }
    }
    if (zfOut)
        for (size_t i = 0; i + 1 < nfilt; ++i) zfOut[i] = z[i];
    return out;
}

} // namespace

// ── filter ────────────────────────────────────────────────────────────
Value filter(const Value &b, const Value &a, const Value &x, std::pmr::memory_resource *mr)
{
    const size_t nb = b.numel(), na = a.numel(), nx = x.numel();
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const double *xd = x.doubleData();

    const double a0 = ad[0];
    if (a0 == 0.0)
        throw Error("filter: a(1) must be nonzero",
                     0, 0, "filter", "", "numkit:filter:zeroLead");

    ScratchArena scratch(mr);
    auto bn = ScratchVec<double>(nb, &scratch);
    auto an = ScratchVec<double>(na, &scratch);
    for (size_t i = 0; i < nb; ++i)
        bn[i] = bd[i] / a0;
    for (size_t i = 0; i < na; ++i)
        an[i] = ad[i] / a0;

    auto out = applyFilterDf2t(bn.data(), nb, an.data(), na, xd, nx, &scratch);

    auto r = createLike(x, ValueType::DOUBLE, mr);
    double *y = r.doubleDataMut();
    std::memcpy(y, out.data(), nx * sizeof(double));
    return r;
}

// ── filtfilt ──────────────────────────────────────────────────────────
Value filtfilt(const Value &b, const Value &a, const Value &x, std::pmr::memory_resource *mr)
{
    const size_t nb = b.numel(), na = a.numel(), nx = x.numel();
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const double *xd = x.doubleData();

    const double a0 = ad[0];
    if (a0 == 0.0)
        throw Error("filtfilt: a(1) must be nonzero",
                     0, 0, "filtfilt", "", "numkit:filtfilt:zeroLead");

    ScratchArena scratch(mr);
    auto bn = ScratchVec<double>(nb, &scratch);
    auto an = ScratchVec<double>(na, &scratch);
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
    auto ext = ScratchVec<double>(extLen, &scratch);
    for (size_t i = 0; i < nEdge; ++i)
        ext[i] = 2.0 * xd[0] - xd[nEdge - i];
    for (size_t i = 0; i < nx; ++i)
        ext[nEdge + i] = xd[i];
    for (size_t i = 0; i < nEdge; ++i)
        ext[nEdge + nx + i] = 2.0 * xd[nx - 1] - xd[nx - 2 - i];

    auto fwd = applyFilterDf2t(bn.data(), nb, an.data(), na, ext.data(), extLen, &scratch);
    std::reverse(fwd.begin(), fwd.end());
    auto bwd = applyFilterDf2t(bn.data(), nb, an.data(), na, fwd.data(), fwd.size(), &scratch);
    std::reverse(bwd.begin(), bwd.end());

    auto r = createLike(x, ValueType::DOUBLE, mr);
    double *y = r.doubleDataMut();
    std::memcpy(y, bwd.data() + nEdge, nx * sizeof(double));
    return r;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void filter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("filter: requires 3 arguments",
                     0, 0, "filter", "", "numkit:filter:nargin");
    // Fast path: plain filter(b,a,x) with one output.
    auto *mr = ctx.engine->resource();
    const bool hasZi = (args.size() >= 4 && !args[3].isEmpty()
                        && !args[3].isChar() && !args[3].isString());
    if (!hasZi && nargout <= 1) {
        outs[0] = filter(args[0], args[1], args[2], mr);
        return;
    }
    // [y, zf] = filter(b,a,x[,zi]): thread initial conditions zi through the
    // DF2T state and return the final state zf (length max(na,nb)-1).
    const Value &b = args[0], &a = args[1], &x = args[2];
    const size_t nb = b.numel(), na = a.numel(), nx = x.numel();
    const double *bd = b.doubleData(), *ad = a.doubleData(), *xd = x.doubleData();
    const double a0 = ad[0];
    if (a0 == 0.0)
        throw Error("filter: a(1) must be nonzero",
                     0, 0, "filter", "", "numkit:filter:zeroLead");
    ScratchArena scratch(mr);
    auto bn = ScratchVec<double>(nb, &scratch);
    auto an = ScratchVec<double>(na, &scratch);
    for (size_t i = 0; i < nb; ++i) bn[i] = bd[i] / a0;
    for (size_t i = 0; i < na; ++i) an[i] = ad[i] / a0;
    const size_t nfilt = std::max(nb, na);
    const size_t zfLen = (nfilt > 0) ? nfilt - 1 : 0;
    const double *ziPtr = hasZi ? args[3].doubleData() : nullptr;
    const size_t  ziLen = hasZi ? args[3].numel() : 0;
    ScratchVec<double> zf(zfLen, &scratch);
    auto out = applyFilterDf2t(bn.data(), nb, an.data(), na, xd, nx, &scratch,
                               ziPtr, ziLen, zfLen ? zf.data() : nullptr);
    Value r = createLike(x, ValueType::DOUBLE, mr);
    if (nx) std::memcpy(r.doubleDataMut(), out.data(), nx * sizeof(double));
    outs[0] = std::move(r);
    if (nargout > 1) {
        Value zfv = Value::matrix(zfLen, 1, ValueType::DOUBLE, mr);
        if (zfLen) std::memcpy(zfv.doubleDataMut(), zf.data(), zfLen * sizeof(double));
        outs[1] = std::move(zfv);
    }
}

void filtfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("filtfilt: requires 3 arguments",
                     0, 0, "filtfilt", "", "numkit:filtfilt:nargin");
    outs[0] = filtfilt(args[0], args[1], args[2], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
