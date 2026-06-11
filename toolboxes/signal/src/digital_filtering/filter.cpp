// toolboxes/signal/src/digital_filtering/filter.cpp

#include <numkit/signal/digital_filtering/filter.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include "filter_detail.hpp"

#include <algorithm>
#include <cstring>
#include <memory_resource>

namespace numkit::signal {

// [Phase 2b] raw-buffer filter kernels promoted to numkit::signal scope so
// filter_reg.cpp can reuse them (declared in filter_detail.hpp).

// Direct Form II transposed core, applied to a flat input buffer.
// Used by both filter() and filtfilt()'s forward/backward passes.
// Optional `zi` (length ziLen) seeds the initial delay state; when
// `zfOut` is non-null the final state (length nfilt-1) is written there —
// this implements MATLAB's filter(b,a,x,zi) and [y,zf] = filter(...).
ScratchVec<double> applyFilterDf2t(const double *bn, size_t nb, const double *an, size_t na, const double *input, size_t len, std::pmr::memory_resource *mr, const double *zi, size_t ziLen, double *zfOut)
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

// Complex Direct Form II transposed core. filter is BILINEAR (the recursive
// a-part mixes terms), so a real/imag split does NOT apply — the recurrence
// runs over Complex. b/a are already a0-normalised.
ScratchVec<Complex> applyFilterDf2tComplex(const Complex *bn, size_t nb, const Complex *an, size_t na, const Complex *input, size_t len, std::pmr::memory_resource *mr, const Complex *zi, size_t ziLen, Complex *zfOut)
{
    const size_t nfilt = std::max(nb, na);
    ScratchVec<Complex> out(len, mr);
    ScratchVec<Complex> z(nfilt, mr);
    const Complex zero(0.0, 0.0);
    for (size_t i = 0; i < nfilt; ++i)
        z[i] = (zi && i < ziLen) ? zi[i] : zero;
    for (size_t n = 0; n < len; ++n) {
        out[n] = (nb > 0 ? bn[0] : zero) * input[n] + z[0];
        for (size_t i = 1; i < nfilt; ++i) {
            z[i - 1] = (i < nb ? bn[i] : zero) * input[n]
                       - (i < na ? an[i] : zero) * out[n]
                       + (i < nfilt - 1 ? z[i] : zero);
        }
    }
    if (zfOut)
        for (size_t i = 0; i + 1 < nfilt; ++i) zfOut[i] = z[i];
    return out;
}

// Gather a Value into a Complex buffer (real element -> Complex(v, 0)).
ScratchVec<Complex> toComplexBuf(const Value &v, size_t n, std::pmr::memory_resource *mr)
{
    ScratchVec<Complex> out(n, mr);
    if (v.isComplex()) {
        const Complex *c = v.complexData();
        for (size_t i = 0; i < n; ++i) out[i] = c[i];
    } else {
        for (size_t i = 0; i < n; ++i) out[i] = Complex(v.elemAsDouble(i), 0.0);
    }
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

// [Phase 2b] end promoted filter kernels

// ── filter ────────────────────────────────────────────────────────────
Value filter(const Value &b, const Value &a, const Value &x, std::pmr::memory_resource *mr)
{
    const size_t nb = b.numel(), na = a.numel(), nx = x.numel();

    // Complex b/a/x: run the recurrence over Complex (a0-normalised).
    if (b.isComplex() || a.isComplex() || x.isComplex()) {
        ScratchArena cs(mr);
        auto bc = toComplexBuf(b, nb, &cs);
        auto ac = toComplexBuf(a, na, &cs);
        auto xc = toComplexBuf(x, nx, &cs);
        const Complex a0 = ac[0];
        if (a0 == Complex(0.0, 0.0))
            throw Error("filter: a(1) must be nonzero",
                         0, 0, "filter", "", "numkit:filter:zeroLead");
        for (size_t i = 0; i < nb; ++i) bc[i] /= a0;
        for (size_t i = 0; i < na; ++i) ac[i] /= a0;
        auto rc = createLike(x, ValueType::COMPLEX, mr);
        Complex *yc = rc.complexDataMut();
        if (nx == 0) return rc;
        const size_t Lc = firstNonSingletonExtent(x);
        for (size_t off = 0; off < nx; off += Lc) {
            auto out = applyFilterDf2tComplex(bc.data(), nb, ac.data(), na,
                                              xc.data() + off, Lc, &cs);
            for (size_t i = 0; i < Lc; ++i) yc[off + i] = out[i];
        }
        return rc;
    }

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

} // namespace numkit::signal
