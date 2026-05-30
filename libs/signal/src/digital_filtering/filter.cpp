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

// MATLAB filter() operates along the first array dimension of x whose size
// is not 1. Because the data is stored column-major, every "signal" along
// that dimension is a contiguous run of `L` samples (all lower dimensions
// have size 1, so their combined stride is 1) and the whole buffer splits
// cleanly into numel/L such runs. Returns L; for a scalar/empty input it
// returns numel (a single — possibly empty — run).
size_t firstNonSingletonExtent(const Value &x)
{
    const Dims &d = x.dims();
    const int nd = d.ndim();
    for (int k = 0; k < nd; ++k)
        if (d.dim(k) != 1) return d.dim(k);
    return x.numel();
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

    auto r = createLike(x, ValueType::DOUBLE, mr);
    double *y = r.doubleDataMut();
    if (nx == 0)
        return r;

    // Filter each signal (run along the first non-singleton dimension)
    // independently, resetting the delay state between runs. For a vector
    // or scalar this is a single run of length nx — identical to before.
    const size_t L = firstNonSingletonExtent(x);
    for (size_t off = 0; off < nx; off += L) {
        auto out = applyFilterDf2t(bn.data(), nb, an.data(), na, xd + off, L, &scratch);
        std::memcpy(y + off, out.data(), L * sizeof(double));
    }
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

    // Per-signal filtering along the first non-singleton dimension (matches
    // MATLAB). Each signal is a contiguous run of length L; the final state
    // zf becomes a (zfLen x nSignals) matrix, one column per run. A vector
    // input is a single run, so zf stays a (zfLen x 1) column — backward
    // compatible. zi may seed every run identically (length zfLen) or
    // per-run (length zfLen*nSignals).
    const size_t L = (nx == 0) ? 0 : firstNonSingletonExtent(x);
    const size_t nSig = (L > 0) ? nx / L : 0;
    const bool ziPerSig = hasZi && zfLen > 0 && nSig > 1 && ziLen == zfLen * nSig;

    Value r = createLike(x, ValueType::DOUBLE, mr);
    double *y = r.doubleDataMut();

    const size_t zfCols = (nSig > 0) ? nSig : 1;
    Value zfv = Value::matrix(zfLen, zfCols, ValueType::DOUBLE, mr);
    double *zfData = zfv.doubleDataMut();
    for (size_t i = 0; i < zfLen * zfCols; ++i) zfData[i] = 0.0;
    // Empty input: the "final state" is just the seed zi (or zeros).
    if (nSig == 0 && hasZi && zfLen)
        for (size_t i = 0; i < zfLen && i < ziLen; ++i) zfData[i] = ziPtr[i];

    for (size_t s = 0; s < nSig; ++s) {
        const double *ziRun = nullptr;
        size_t ziRunLen = 0;
        if (hasZi) {
            ziRun    = ziPerSig ? ziPtr + s * zfLen : ziPtr;
            ziRunLen = ziPerSig ? zfLen : ziLen;
        }
        ScratchVec<double> zf(zfLen, &scratch);
        auto out = applyFilterDf2t(bn.data(), nb, an.data(), na, xd + s * L, L,
                                   &scratch, ziRun, ziRunLen,
                                   zfLen ? zf.data() : nullptr);
        std::memcpy(y + s * L, out.data(), L * sizeof(double));
        if (zfLen) std::memcpy(zfData + s * zfLen, zf.data(), zfLen * sizeof(double));
    }

    outs[0] = std::move(r);
    if (nargout > 1)
        outs[1] = std::move(zfv);
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
