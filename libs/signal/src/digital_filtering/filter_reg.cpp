// libs/signal/src/digital_filtering/filter_reg.cpp
//
// Register half of the signal filter builtins: the CallContext wrappers
// delegating to the engine-free compute in filter.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/digital_filtering/filter.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "helpers.hpp"
#include "filter_detail.hpp"
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <cstring>
#include <vector>

namespace numkit::signal {

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

    // Complex [y,zf] / zi path: same DF2T recurrence over Complex, with a
    // complex final-state zf and (optionally complex) initial state zi.
    if (b.isComplex() || a.isComplex() || x.isComplex()
        || (hasZi && args[3].isComplex())) {
        ScratchArena cs(mr);
        auto bc = toComplexBuf(b, nb, &cs);
        auto ac = toComplexBuf(a, na, &cs);
        auto xc = toComplexBuf(x, nx, &cs);
        const Complex a0c = ac[0];
        if (a0c == Complex(0.0, 0.0))
            throw Error("filter: a(1) must be nonzero",
                         0, 0, "filter", "", "numkit:filter:zeroLead");
        for (size_t i = 0; i < nb; ++i) bc[i] /= a0c;
        for (size_t i = 0; i < na; ++i) ac[i] /= a0c;
        const size_t nfiltC = std::max(nb, na);
        const size_t zfLenC = (nfiltC > 0) ? nfiltC - 1 : 0;
        const size_t ziLenC = hasZi ? args[3].numel() : 0;
        ScratchVec<Complex> zic = hasZi ? toComplexBuf(args[3], ziLenC, &cs)
                                        : ScratchVec<Complex>(0, &cs);
        const Complex *ziPtrC = hasZi ? zic.data() : nullptr;
        const size_t Lc = (nx == 0) ? 0 : firstNonSingletonExtent(x);
        const size_t nSigC = (Lc > 0) ? nx / Lc : 0;
        const bool ziPerSigC = hasZi && zfLenC > 0 && nSigC > 1 && ziLenC == zfLenC * nSigC;

        Value rc = createLike(x, ValueType::COMPLEX, mr);
        Complex *yc = rc.complexDataMut();
        const size_t zfColsC = (nSigC > 0) ? nSigC : 1;
        Value zfvc = Value::matrix(zfLenC, zfColsC, ValueType::COMPLEX, mr);
        Complex *zfDataC = zfvc.complexDataMut();
        for (size_t i = 0; i < zfLenC * zfColsC; ++i) zfDataC[i] = Complex(0.0, 0.0);
        if (nSigC == 0 && hasZi && zfLenC)
            for (size_t i = 0; i < zfLenC && i < ziLenC; ++i) zfDataC[i] = ziPtrC[i];
        for (size_t s = 0; s < nSigC; ++s) {
            const Complex *ziRun = nullptr;
            size_t ziRunLen = 0;
            if (hasZi) {
                ziRun    = ziPerSigC ? ziPtrC + s * zfLenC : ziPtrC;
                ziRunLen = ziPerSigC ? zfLenC : ziLenC;
            }
            ScratchVec<Complex> zf(zfLenC, &cs);
            auto out = applyFilterDf2tComplex(bc.data(), nb, ac.data(), na,
                                              xc.data() + s * Lc, Lc, &cs,
                                              ziRun, ziRunLen,
                                              zfLenC ? zf.data() : nullptr);
            for (size_t i = 0; i < Lc; ++i) yc[s * Lc + i] = out[i];
            if (zfLenC)
                for (size_t i = 0; i < zfLenC; ++i) zfDataC[s * zfLenC + i] = zf[i];
        }
        outs[0] = std::move(rc);
        if (nargout > 1) outs[1] = std::move(zfvc);
        return;
    }

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
