// toolboxes/.../interp_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by interp.cpp + interp_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/reductions.hpp>  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::builtin {

namespace {

size_t findInterval(const double *xData, size_t n, double xq)
{
    if (xq <= xData[0])
        return 0;
    if (xq >= xData[n - 1])
        return n - 2;
    auto it = std::upper_bound(xData, xData + n, xq);
    size_t idx = static_cast<size_t>(it - xData);
    if (idx == 0)
        return 0;
    return idx - 1;
}

ScratchVec<double>
interpLinear(const double *x, const double *y, size_t n, const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);
        const double dx = x[i + 1] - x[i];
        if (dx == 0.0) {
            yq[k] = y[i];
        } else {
            const double t = (xq[k] - x[i]) / dx;
            yq[k] = y[i] + t * (y[i + 1] - y[i]);
        }
    }
    return yq;
}

ScratchVec<double>
interpNearest(const double *x, const double *y, size_t n, const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);
        // Tie-break: MATLAB rounds an exactly-halfway query UP to the
        // higher-index neighbor, so use strict '<' (a tie picks y[i+1]).
        if (std::abs(xq[k] - x[i]) < std::abs(xq[k] - x[i + 1]))
            yq[k] = y[i];
        else
            yq[k] = y[i + 1];
    }
    return yq;
}

// 'previous': value at the largest knot <= xq. 'next': value at the
// smallest knot >= xq. Queries outside [x[0], x[n-1]] -> NaN (MATLAB's
// default, no extrapolation). x is assumed ascending.
ScratchVec<double>
interpPrevious(const double *x, const double *y, size_t n, const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const double q = xq[k];
        if (q < x[0] || q > x[n - 1]) { yq[k] = NaN; continue; }
        size_t i = 0;
        for (size_t j = 0; j < n && x[j] <= q; ++j) i = j;
        yq[k] = y[i];
    }
    return yq;
}

ScratchVec<double>
interpNext(const double *x, const double *y, size_t n, const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const double q = xq[k];
        if (q < x[0] || q > x[n - 1]) { yq[k] = NaN; continue; }
        size_t i = n - 1;
        for (size_t j = 0; j < n; ++j) { if (x[j] >= q) { i = j; break; } }
        yq[k] = y[i];
    }
    return yq;
}

// Build the n-vector of second derivatives at knots (sigma) using
// MATLAB's not-a-knot boundary conditions. n == 3 is handled separately
// (NaK reduces to the interpolating parabola). Tridiagonal system has the standard
// interior rows (j=1..n-2) with the first/last rows modified to
// substitute sigma_0 / sigma_{n-1} with their NaK linear combinations.
ScratchVec<double>
computeSplineSigma(const double *x, const double *y, size_t n, const double *h, std::pmr::memory_resource *mr)
{
    ScratchVec<double> sigma(n, 0.0, mr);
    if (n < 3) return sigma;

    // n == 3: not-a-knot reduces to the unique interpolating PARABOLA, whose
    // second derivative is the constant 2*f[x0,x1,x2] (MATLAB's convention).
    // The general tridiagonal path below would leave the boundary sigmas at 0
    // (natural BCs) for n==3, which diverges from MATLAB (e.g. spline of x^2
    // on 3 points must reproduce x^2 exactly).
    if (n == 3) {
        const double c = 2.0 * ((y[2] - y[1]) / h[1] - (y[1] - y[0]) / h[0])
                             / (h[0] + h[1]);
        sigma[0] = c;
        sigma[1] = c;
        sigma[2] = c;
        return sigma;
    }

    const size_t m = n - 2;
    ScratchVec<double> diag(m, mr), upper(m, mr), lower(m, mr), rhs(m, mr);

    for (size_t i = 0; i < m; ++i) {
        const size_t j = i + 1;
        diag[i] = 2.0 * (h[j - 1] + h[j]);
        rhs[i] = 6.0 * ((y[j + 1] - y[j]) / h[j] - (y[j] - y[j - 1]) / h[j - 1]);
        if (i > 0)         lower[i] = h[j - 1];
        if (i < m - 1)     upper[i] = h[j];
    }

    // Not-a-knot modifications (n >= 4 here; n==3 handled above).
    // See BUGS.md note attached to spline parity:
    //   sigma_0 = ((h_0 + h_1) * s_1 - h_0 * s_2) / h_1
    //   sigma_{n-1} = ((h_{n-3} + h_{n-2}) * s_{n-2}
    //                  - h_{n-2} * s_{n-3}) / h_{n-3}
    // Substitute into rows 0 and m-1 of the standard tridiagonal.
    if (m >= 2) {
        const double h0 = h[0];
        const double h1 = h[1];
        diag[0]  = (h0 + h1) * (h0 + 2.0 * h1) / h1;
        upper[0] = (h1 * h1 - h0 * h0) / h1;

        const double hL  = h[n - 3];   // h_{n-3}
        const double hL1 = h[n - 2];   // h_{n-2}
        diag[m - 1]  = (hL + hL1) * (2.0 * hL + hL1) / hL;
        lower[m - 1] = (hL * hL - hL1 * hL1) / hL;
    }

    for (size_t i = 1; i < m; ++i) {
        const double w = lower[i] / diag[i - 1];
        diag[i] -= w * upper[i - 1];
        rhs[i]  -= w * rhs[i - 1];
    }

    sigma[m] = rhs[m - 1] / diag[m - 1];
    for (int i = static_cast<int>(m) - 2; i >= 0; --i)
        sigma[i + 1] = (rhs[i] - upper[i] * sigma[i + 2]) / diag[i];

    // Recover boundary sigmas from the NaK linear combinations.
    if (m >= 2) {
        const double h0 = h[0];
        const double h1 = h[1];
        sigma[0] = ((h0 + h1) * sigma[1] - h0 * sigma[2]) / h1;
        const double hL  = h[n - 3];
        const double hL1 = h[n - 2];
        sigma[n - 1] = ((hL + hL1) * sigma[n - 2] - hL1 * sigma[n - 3]) / hL;
    }
    return sigma;
}

ScratchVec<double>
interpSpline(const double *x, const double *y, size_t n, const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq, mr);

    const size_t nm1 = n - 1;

    ScratchVec<double> h(nm1, mr);
    for (size_t i = 0; i < nm1; ++i)
        h[i] = x[i + 1] - x[i];

    auto sigma = computeSplineSigma(x, y, n, h.data(), mr);

    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);
        const double dx = xq[k] - x[i];
        const double dx1 = x[i + 1] - xq[k];
        const double hi = h[i];

        yq[k] = sigma[i] * dx1 * dx1 * dx1 / (6.0 * hi)
                + sigma[i + 1] * dx * dx * dx / (6.0 * hi)
                + (y[i] / hi - sigma[i] * hi / 6.0) * dx1
                + (y[i + 1] / hi - sigma[i + 1] * hi / 6.0) * dx;
    }
    return yq;
}

ScratchVec<double>
interpPchip(const double *x, const double *y, size_t n, const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq, mr);

    const size_t nm1 = n - 1;

    ScratchVec<double> h(nm1, mr), delta(nm1, mr);
    for (size_t i = 0; i < nm1; ++i) {
        h[i] = x[i + 1] - x[i];
        delta[i] = (y[i + 1] - y[i]) / h[i];
    }

    ScratchVec<double> d(n, 0.0, mr);

    for (size_t i = 1; i < nm1; ++i) {
        if (delta[i - 1] * delta[i] <= 0.0) {
            d[i] = 0.0;
        } else {
            const double w1 = 2.0 * h[i] + h[i - 1];
            const double w2 = h[i] + 2.0 * h[i - 1];
            d[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]);
        }
    }

    d[0] = ((2.0 * h[0] + h[1]) * delta[0] - h[0] * delta[1]) / (h[0] + h[1]);
    if (d[0] * delta[0] < 0.0)
        d[0] = 0.0;
    else if (delta[0] * delta[1] < 0.0 && std::abs(d[0]) > std::abs(3.0 * delta[0]))
        d[0] = 3.0 * delta[0];

    d[nm1] = ((2.0 * h[nm1 - 1] + h[nm1 - 2]) * delta[nm1 - 1] - h[nm1 - 1] * delta[nm1 - 2])
             / (h[nm1 - 1] + h[nm1 - 2]);
    if (d[nm1] * delta[nm1 - 1] < 0.0)
        d[nm1] = 0.0;
    else if (delta[nm1 - 2] * delta[nm1 - 1] < 0.0
             && std::abs(d[nm1]) > std::abs(3.0 * delta[nm1 - 1]))
        d[nm1] = 3.0 * delta[nm1 - 1];

    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);
        const double t = (xq[k] - x[i]) / h[i];
        const double t2 = t * t;
        const double t3 = t2 * t;

        const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        const double h10 = t3 - 2.0 * t2 + t;
        const double h01 = -2.0 * t3 + 3.0 * t2;
        const double h11 = t3 - t2;

        yq[k] = h00 * y[i] + h10 * h[i] * d[i] + h01 * y[i + 1] + h11 * h[i] * d[i + 1];
    }
    return yq;
}

// ── Modified Akima (`makima`) ─────────────────────────────────────────
//
// Akima 1970 cubic Hermite interpolation with the Akima-2 / "modified"
// weight that adds `|m_{i+1} + m_i| / 2` to the standard weight
// `|m_{i+1} - m_i|`. The extra |sum|/2 term avoids zero-weight
// degeneracies on flat (m == 0) or co-linear data.
//
// Weights per interior derivative d_i (using slopes m_{-1}..m_{n+1}):
//   w1 = |m_{i+1} - m_i|     + |m_{i+1} + m_i|     / 2
//   w2 = |m_{i-1} - m_{i-2}| + |m_{i-1} + m_{i-2}| / 2
//   d_i = (w1 * m_{i-1} + w2 * m_i) / (w1 + w2)     [0 if w1+w2 == 0]
//
// Boundary slopes m_{-1}, m_0, m_n, m_{n+1} use Akima's quadratic
// extrapolation: m_0 = 2*m_1 - m_2; m_{-1} = 2*m_0 - m_1; symmetric at
// the right edge.
ScratchVec<double>
interpMakima(const double *x, const double *y, size_t n,
              const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    if (n < 2)
        throw Error("makima: need at least 2 data points",
                     0, 0, "makima", "", "numkit:makima:tooFewPoints");
    if (n < 3)
        return interpLinear(x, y, n, xq, nq, mr);

    const size_t nm1 = n - 1;

    ScratchVec<double> h(nm1, mr);
    for (size_t i = 0; i < nm1; ++i)
        h[i] = x[i + 1] - x[i];

    // Slopes m[0..n-2] = (y[i+1] - y[i]) / h[i]. Extend by 2 on each
    // side using Akima's quadratic extrapolation. Store in mExt of
    // length n+3 with indexing offset = 2 (so mExt[2 + i] == m[i] for
    // i ∈ [-2 .. n], where m[-1], m[-2], m[n-1], m[n] are extrapolated).
    ScratchVec<double> mExt(n + 3, mr);
    for (size_t i = 0; i < nm1; ++i)
        mExt[2 + i] = (y[i + 1] - y[i]) / h[i];

    // Quadratic extrapolation at the left:  m[-1] = 2*m[0] - m[1]
    //                                       m[-2] = 2*m[-1] - m[0]
    mExt[1] = 2.0 * mExt[2] - mExt[3];
    mExt[0] = 2.0 * mExt[1] - mExt[2];
    // Right side: m[n-1] = 2*m[n-2] - m[n-3]
    //             m[n]   = 2*m[n-1] - m[n-2]
    mExt[2 + nm1]     = 2.0 * mExt[2 + nm1 - 1] - mExt[2 + nm1 - 2];
    mExt[2 + nm1 + 1] = 2.0 * mExt[2 + nm1]     - mExt[2 + nm1 - 1];

    // Derivative at each data point i ∈ [0..n-1].
    ScratchVec<double> d(n, mr);
    for (size_t i = 0; i < n; ++i) {
        // mExt indices for the 4 slopes around point i:
        //   ml2 = m[i - 2], ml1 = m[i - 1], mr1 = m[i], mr2 = m[i + 1]
        const double ml2 = mExt[i];
        const double ml1 = mExt[i + 1];
        const double mr1 = mExt[i + 2];
        const double mr2 = mExt[i + 3];
        const double w1 = std::abs(mr2 - mr1) + std::abs(mr2 + mr1) * 0.5;
        const double w2 = std::abs(ml1 - ml2) + std::abs(ml1 + ml2) * 0.5;
        const double wsum = w1 + w2;
        d[i] = (wsum == 0.0) ? 0.0 : (w1 * ml1 + w2 * mr1) / wsum;
    }

    // Evaluate with cubic Hermite basis (same as pchip).
    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);
        const double t = (xq[k] - x[i]) / h[i];
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double h00 =  2.0 * t3 - 3.0 * t2 + 1.0;
        const double h10 =        t3 - 2.0 * t2 + t;
        const double h01 = -2.0 * t3 + 3.0 * t2;
        const double h11 =        t3 -       t2;
        yq[k] = h00 * y[i]     + h10 * h[i] * d[i]
              + h01 * y[i + 1] + h11 * h[i] * d[i + 1];
    }
    return yq;
}

// ── v5cubic / cubic (1-D Keys cubic convolution) ──────────────────────
//
// MATLAB's interp1(...,'v5cubic') and (...,'cubic') use the classic Keys
// (a=-0.5) cubic convolution on a UNIFORMLY-spaced grid. On a non-uniform
// grid MATLAB warns and switches to 'spline', so we delegate there. The
// one-cell boundary is the MATLAB cubic extrapolation 3·y1-3·y2+y3 (same
// padding interp2 'cubic' uses). Out-of-range queries return NaN — the
// caller's Default extrapolation policy enforces that ('cubic'/'v5cubic'
// are NOT method-extrapolators).
inline double keys1d(double s)
{
    s = std::fabs(s);
    if (s <= 1.0) return ((1.5 * s - 2.5) * s) * s + 1.0;
    if (s <  2.0) return (((-0.5 * s + 2.5) * s) - 4.0) * s + 2.0;
    return 0.0;
}

ScratchVec<double>
interpV5Cubic(const double *x, const double *y, size_t n,
              const double *xq, size_t nq, std::pmr::memory_resource *mr)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq, mr);

    // Uniform-grid check; fall back to spline (matching MATLAB) otherwise.
    const double step = x[1] - x[0];
    bool uniform = true;
    for (size_t i = 2; i < n; ++i)
        if (std::abs((x[i] - x[i - 1]) - step) > 1e-10 * std::max(1.0, std::abs(step))) {
            uniform = false;
            break;
        }
    if (!uniform)
        return interpSpline(x, y, n, xq, nq, mr);

    // One-cell padded copy: ypad[j+1] = y[j]; borders = 3·v1-3·v2+v3.
    ScratchVec<double> ypad(n + 2, mr);
    for (size_t j = 0; j < n; ++j) ypad[j + 1] = y[j];
    ypad[0]     = 3.0 * y[0]     - 3.0 * y[1]     + y[2];
    ypad[n + 1] = 3.0 * y[n - 1] - 3.0 * y[n - 2] + y[n - 3];

    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);   // clamped cell; OOR NaN'd by caller
        const double t = (xq[k] - x[i]) / step;
        const double w0 = keys1d(1.0 + t);
        const double w1 = keys1d(t);
        const double w2 = keys1d(1.0 - t);
        const double w3 = keys1d(2.0 - t);
        yq[k] = w0 * ypad[i] + w1 * ypad[i + 1] + w2 * ypad[i + 2] + w3 * ypad[i + 3];
    }
    return yq;
}

// Helper for interp1 / spline / pchip — pack a yq buffer into a Value
// preserving xq's row/column orientation.
Value packInterpResult(const double *yq, std::size_t nq,
                       const Value &xq, std::pmr::memory_resource *mr)
{
    const bool isRow = xq.dims().rows() == 1;
    auto r = isRow ? Value::matrix(1, nq, ValueType::DOUBLE, mr)
                   : Value::matrix(nq, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nq; ++i)
        r.doubleDataMut()[i] = yq[i];
    return r;
}

// Out-of-range (extrapolation) policy for interp1.
//   Default — MATLAB's default: 'spline'/'pchip'/'makima' extrapolate using
//             the method; every other method returns NaN outside [x0, xN-1].
//   Method  — the literal 'extrap' option: extrapolate using the method for
//             all methods.
//   Const   — a numeric extrapval: fill every out-of-range query with it.
enum class Interp1Extrap { Default, Method, Const };

// Rewrite out-of-range entries of a computed query buffer per the policy.
// `xd` is assumed ascending; `yd` is needed to hold the endpoint value for
// 'previous'/'next' under the Method ('extrap') option. The interpolation
// helpers already produce method-extrapolated values out-of-range for
// linear/nearest/spline/pchip/makima (findInterval clamps to the boundary
// interval); previous/next emit NaN, so they get special handling.
void applyInterp1Extrap(double *yq, const double *xqd, size_t nq,
                        const double *xd, const double *yd, size_t n,
                        const std::string &method, Interp1Extrap mode,
                        double fill)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double lo = xd[0];
    const double hi = xd[n - 1];
    const bool methodExtraps =
        (method == "spline" || method == "pchip" || method == "makima");
    const bool isPrev = (method == "previous");
    const bool isNext = (method == "next");
    for (size_t k = 0; k < nq; ++k) {
        const double q = xqd[k];
        if (q >= lo && q <= hi)
            continue; // interior — the helper value already stands
        switch (mode) {
        case Interp1Extrap::Const:
            yq[k] = fill;
            break;
        case Interp1Extrap::Method:
            // 'previous'/'next' hold the endpoint on the side that has a
            // sample; the opposite side has no such sample → NaN (matches
            // MATLAB: interp1(x,y,4,'previous','extrap')=y(end),
            // interp1(x,y,0,'previous','extrap')=NaN).
            if (isPrev)
                yq[k] = (q > hi) ? yd[n - 1] : NaN;
            else if (isNext)
                yq[k] = (q < lo) ? yd[0] : NaN;
            // else: linear/nearest/spline/pchip/makima already extrapolated.
            break;
        case Interp1Extrap::Default:
            if (!methodExtraps)
                yq[k] = NaN;
            break;
        }
    }
}

// Shared interp1 core: dispatch on method, then apply the extrapolation
// policy. Both the public interp1() and interp1_reg() funnel through here.
Value interp1Dispatch(const Value &x, const Value &y, const Value &xq,
                      const std::string &method, Interp1Extrap mode,
                      double fill, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    const size_t nq = xq.numel();

    if (n < 2)
        throw Error("interp1: need at least 2 data points",
                     0, 0, "interp1", "", "numkit:interp1:tooFewPoints");

    // Complex y: interpolate the real and imaginary parts separately and
    // recombine (MATLAB). This wraps every method + the matrix-column path,
    // and inherits the same extrapolation policy (NaN out-of-range → NaN+NaNi).
    if (y.type() == ValueType::COMPLEX) {
        const size_t m = y.numel();
        Value yr = createLike(y, ValueType::DOUBLE, mr);
        Value yi = createLike(y, ValueType::DOUBLE, mr);
        double *rp = yr.doubleDataMut();
        double *ip = yi.doubleDataMut();
        const Complex *c = y.complexData();
        for (size_t i = 0; i < m; ++i) { rp[i] = c[i].real(); ip[i] = c[i].imag(); }
        Value rr = interp1Dispatch(x, yr, xq, method, mode, fill, mr);
        Value ri = interp1Dispatch(x, yi, xq, method, mode, fill, mr);
        Value out = createLike(rr, ValueType::COMPLEX, mr);
        Complex *o = out.complexDataMut();
        const double *re = rr.doubleData(), *im = ri.doubleData();
        const size_t no = out.numel();
        for (size_t i = 0; i < no; ++i) o[i] = Complex(re[i], im[i]);
        return out;
    }

    const double *xd = x.doubleData();
    const double *xqd = xq.doubleData();

    ScratchArena scratch(mr);

    // Interpolate a single y-data column of length n, applying the
    // out-of-range extrapolation policy. Returns the nq query values.
    auto runColumn = [&](const double *yd) -> ScratchVec<double> {
        ScratchVec<double> yq = [&]() -> ScratchVec<double> {
            if (method == "linear")   return interpLinear(xd, yd, n, xqd, nq, &scratch);
            if (method == "nearest")  return interpNearest(xd, yd, n, xqd, nq, &scratch);
            if (method == "previous") return interpPrevious(xd, yd, n, xqd, nq, &scratch);
            if (method == "next")     return interpNext(xd, yd, n, xqd, nq, &scratch);
            if (method == "spline")   return interpSpline(xd, yd, n, xqd, nq, &scratch);
            if (method == "pchip")    return interpPchip(xd, yd, n, xqd, nq, &scratch);
            if (method == "makima")   return interpMakima(xd, yd, n, xqd, nq, &scratch);
            if (method == "cubic" || method == "v5cubic")
                // Keys cubic convolution on a uniform grid (spline on
                // non-uniform); out-of-range → NaN (NOT a method-
                // extrapolator, see applyInterp1Extrap).
                return interpV5Cubic(xd, yd, n, xqd, nq, &scratch);
            throw Error("interp1: unknown method '" + method + "'",
                         0, 0, "interp1", "", "numkit:interp1:badMethod");
        }();
        applyInterp1Extrap(yq.data(), xqd, nq, xd, yd, n, method, mode, fill);
        return yq;
    };

    const bool yIsVector = y.dims().isVector() || y.isScalar();
    if (yIsVector) {
        if (n != y.numel())
            throw Error("interp1: x and y must have same length",
                         0, 0, "interp1", "", "numkit:interp1:lengthMismatch");
        auto yq = runColumn(y.doubleData());
        return packInterpResult(yq.data(), yq.size(), xq, mr);
    }

    // Matrix Y: interp1 operates DOWN each column; size(Y,1) must equal
    // length(X). The result is nq × size(Y,2), regardless of the xq
    // orientation (matches MATLAB). N-D Y is deferred.
    if (y.dims().ndim() > 2)
        throw Error("interp1: N-D Y arrays are not supported in this "
                    "revision (vector or 2-D matrix only)",
                     0, 0, "interp1", "", "numkit:interp1:ndY");
    const size_t yr = static_cast<size_t>(y.dims().dim(0));
    const size_t yc = static_cast<size_t>(y.dims().dim(1));
    if (yr != n)
        throw Error("interp1: for a matrix Y, size(Y,1) must equal length(X)",
                     0, 0, "interp1", "", "numkit:interp1:lengthMismatch");
    const double *yd = y.doubleData();
    auto out = Value::matrix(nq, yc, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t j = 0; j < yc; ++j) {
        auto yq = runColumn(yd + j * yr);   // column j is contiguous (col-major)
        for (size_t i = 0; i < nq; ++i) od[j * nq + i] = yq[i];
    }
    return out;
}

} // anonymous namespace
namespace {

enum class Interp2Method { Linear, Nearest, Cubic, Spline };

// `allowSeparable` enables the tensor-product 'spline' method (interp2
// only). interp3 leaves it false — 'spline' stays unsupported there and
// falls through to the "not yet supported" error as before.
//
// NOTE: 'makima' (and 'pchip') are intentionally NOT enabled here. The
// cubic spline is a LINEAR interpolation operator, so the 2-D result
// equals sequential 1-D interpolation (interpolate along x for each row,
// then along y) — that separable form reproduces MATLAB exactly. makima
// is NONLINEAR (its Hermite derivative weights depend on |slope diffs|),
// so the naive separable form diverges from MATLAB's true tensor-product
// bicubic Hermite (which needs consistent cross ∂²/∂x∂y derivatives) at
// interior points. Implementing that correctly is deferred.
Interp2Method parseInterp2Method(const std::string &m, bool allowSeparable = false)
{
    std::string s = m;
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s.empty() || s == "linear") return Interp2Method::Linear;
    if (s == "nearest")             return Interp2Method::Nearest;
    if (s == "cubic")               return Interp2Method::Cubic;
    if (allowSeparable && s == "spline") return Interp2Method::Spline;
    if (s == "spline" || s == "pchip" || s == "makima")
        throw Error("interp2: '" + m + "' method not yet supported "
                     "(linear / nearest / cubic / spline available)",
                     0, 0, "interp2", "", "numkit:interp2:unsupportedMethod");
    throw Error("interp2: unknown method '" + m + "'",
                 0, 0, "interp2", "", "numkit:interp2:badMethod");
}

// Keys' cubic convolution kernel (a = -0.5).
inline double keysCubic(double s)
{
    s = std::fabs(s);
    if (s <= 1.0) return ((1.5 * s - 2.5) * s) * s + 1.0;       // 1.5s³ - 2.5s² + 1
    if (s <  2.0) return (((-0.5 * s + 2.5) * s) - 4.0) * s + 2.0; // -0.5s³ + 2.5s² - 4s + 2
    return 0.0;
}

// Locate the cell index i such that grid[i] <= q <= grid[i+1]; returns
// SIZE_MAX if q is outside [grid[0], grid[n-1]] (caller emits NaN).
inline std::size_t findCell(const double *grid, std::size_t n, double q)
{
    if (n < 2) return std::size_t(-1);
    if (q < grid[0] || q > grid[n - 1]) return std::size_t(-1);
    // Binary search.
    std::size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        const std::size_t mid = (lo + hi) / 2;
        if (grid[mid] <= q) lo = mid; else hi = mid;
    }
    return lo;
}

void validateMonotonicAscending(const double *g, std::size_t n, const char *axis)
{
    for (std::size_t i = 1; i < n; ++i)
        if (g[i] <= g[i - 1])
            throw Error(std::string("interp2: ") + axis
                         + " must be strictly increasing",
                         0, 0, "interp2", "", "numkit:interp2:notMonotonic");
}

// Bicubic (Keys, a=-0.5) convolution sample. Vpad is the (R+2)×(C+2)
// padded grid (column-major; original element (i,j) lives at (i+1,j+1));
// the one-cell border is the MATLAB cubic extrapolation 3·v1-3·v2+v3.
// Assumes a uniformly-spaced grid (caller validates). NaN out of range.
double cubicSample(const double *Vpad, std::size_t R, std::size_t C,
                   const double *xGrid, const double *yGrid, double xq, double yq)
{
    const std::size_t ix = findCell(xGrid, C, xq);
    const std::size_t iy = findCell(yGrid, R, yq);
    if (ix == std::size_t(-1) || iy == std::size_t(-1))
        return std::nan("");
    const double tx = (xq - xGrid[ix]) / (xGrid[ix + 1] - xGrid[ix]);
    const double ty = (yq - yGrid[iy]) / (yGrid[iy + 1] - yGrid[iy]);
    const double wx[4] = { keysCubic(1.0 + tx), keysCubic(tx),
                           keysCubic(1.0 - tx), keysCubic(2.0 - tx) };
    const double wy[4] = { keysCubic(1.0 + ty), keysCubic(ty),
                           keysCubic(1.0 - ty), keysCubic(2.0 - ty) };
    const std::size_t PR = R + 2;
    double acc = 0.0;
    for (int a = 0; a < 4; ++a) {            // y-neighbours: padded rows iy..iy+3
        double rowAcc = 0.0;
        for (int b = 0; b < 4; ++b)          // x-neighbours: padded cols ix..ix+3
            rowAcc += wx[b] * Vpad[(ix + static_cast<std::size_t>(b)) * PR
                                   + (iy + static_cast<std::size_t>(a))];
        acc += wy[a] * rowAcc;
    }
    return acc;
}

// Fast path: V is column-major (rows = R, cols = C). Sample one bilinear,
// nearest-neighbour, or bicubic value at (xq, yq) using x grid (length C)
// and y grid (length R). For Cubic, Vpad (the padded grid) must be set.
double interp2Sample(const double *V, std::size_t R, std::size_t C,
                     const double *xGrid, const double *yGrid,
                     double xq, double yq, Interp2Method method,
                     const double *Vpad = nullptr)
{
    if (method == Interp2Method::Cubic)
        return cubicSample(Vpad, R, C, xGrid, yGrid, xq, yq);
    const std::size_t ix = findCell(xGrid, C, xq);
    const std::size_t iy = findCell(yGrid, R, yq);
    if (ix == std::size_t(-1) || iy == std::size_t(-1))
        return std::nan("");
    if (method == Interp2Method::Nearest) {
        const std::size_t cx = (xq - xGrid[ix] <= xGrid[ix + 1] - xq) ? ix : ix + 1;
        const std::size_t cy = (yq - yGrid[iy] <= yGrid[iy + 1] - yq) ? iy : iy + 1;
        return V[cx * R + cy];
    }
    // Bilinear. v(r, c) = V[c*R + r].
    const double x0 = xGrid[ix], x1 = xGrid[ix + 1];
    const double y0 = yGrid[iy], y1 = yGrid[iy + 1];
    const double tx = (xq - x0) / (x1 - x0);
    const double ty = (yq - y0) / (y1 - y0);
    const double v00 = V[ix       * R + iy];
    const double v10 = V[(ix + 1) * R + iy];
    const double v01 = V[ix       * R + (iy + 1)];
    const double v11 = V[(ix + 1) * R + (iy + 1)];
    return (1.0 - tx) * (1.0 - ty) * v00
         + tx         * (1.0 - ty) * v10
         + (1.0 - tx) * ty         * v01
         + tx         * ty         * v11;
}

// Extract a 1-D axis vector from possibly meshgrid/ndgrid output.
// Auto-detects which dimension carries the variation (works for both
// meshgrid and ndgrid: X varies along cols (dim 2) in meshgrid, along
// rows (dim 1) in ndgrid). The `axis` hint is used to pick a default
// extraction direction when only one dim is non-trivial; otherwise the
// varying-direction is detected automatically.
void readGridAxis(const Value &g, ScratchVec<double> &out, const char *axis)
{
    if (g.dims().isVector() || g.isScalar()) {
        out.resize(g.numel());
        for (std::size_t i = 0; i < g.numel(); ++i) out[i] = g.elemAsDouble(i);
        return;
    }
    const std::size_t r = g.dims().rows();
    const std::size_t c = g.dims().cols();
    const std::size_t p = g.dims().is3D() ? g.dims().pages() : 1;
    const std::size_t pageStride = r * c;

    // Try each possible varying direction and pick the one whose
    // extracted values are non-constant. For the AXIS hint
    // ("X"/"Y"/"Z") we prefer the conventional meshgrid mapping but
    // fall through to other dims if that one is constant (ndgrid form).
    auto extractAlong = [&](int dim) {
        // dim: 0 = rows (varying along dim 1), 1 = cols (dim 2), 2 = pages (dim 3).
        std::size_t n = (dim == 0) ? r : (dim == 1) ? c : p;
        ScratchVec<double> v(out.get_allocator().resource());
        v.resize(n);
        for (std::size_t k = 0; k < n; ++k) {
            std::size_t flat = (dim == 0) ? k
                              : (dim == 1) ? (k * r)
                              : (k * pageStride);
            v[k] = g.elemAsDouble(flat);
        }
        return v;
    };
    auto isConst = [](const ScratchVec<double> &v) {
        if (v.size() < 2) return true;
        for (std::size_t i = 1; i < v.size(); ++i)
            if (v[i] != v[0]) return false;
        return true;
    };

    const std::string ax = axis;
    int preferred = (ax == "X") ? 1 : (ax == "Y") ? 0 : (ax == "Z") ? 2 : 1;
    for (int tries = 0; tries < 3; ++tries) {
        int dim = (preferred + tries) % 3;
        if (dim == 2 && !g.dims().is3D()) continue;
        auto v = extractAlong(dim);
        if (!isConst(v)) {
            out = std::move(v);
            return;
        }
    }
    // All dims constant -> just emit the (degenerate) preferred extract.
    out = extractAlong(preferred);
}

Value interp2Impl(const Value &V, const double *xGrid, std::size_t xN, const double *yGrid, std::size_t yN, const Value &Xq, const Value &Yq, const std::string &method, std::pmr::memory_resource *mr)
{
    if (V.type() == ValueType::COMPLEX)
        throw Error("interp2: complex inputs are not supported",
                     0, 0, "interp2", "", "numkit:interp2:complex");
    if (V.dims().is3D() || V.dims().ndim() > 2)
        throw Error("interp2: V must be a 2D matrix",
                     0, 0, "interp2", "", "numkit:interp2:rank");

    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    if (xN != C)
        throw Error("interp2: length(X) must equal cols(V)",
                     0, 0, "interp2", "", "numkit:interp2:gridSize");
    if (yN != R)
        throw Error("interp2: length(Y) must equal rows(V)",
                     0, 0, "interp2", "", "numkit:interp2:gridSize");
    validateMonotonicAscending(xGrid, C, "X");
    validateMonotonicAscending(yGrid, R, "Y");

    const Interp2Method m = parseInterp2Method(method, /*allowSeparable=*/true);
    ScratchArena scratch(mr);
    // V as DOUBLE (promote if needed).
    ScratchVec<double> Vd(R * C, &scratch);
    if (V.type() == ValueType::DOUBLE)
        std::memcpy(Vd.data(), V.doubleData(), R * C * sizeof(double));
    else
        for (std::size_t i = 0; i < R * C; ++i) Vd[i] = V.elemAsDouble(i);

    // Bicubic convolution needs a uniformly-spaced grid and a one-cell
    // padded copy (border = MATLAB cubic extrapolation 3·v1-3·v2+v3).
    ScratchVec<double> Vpad(&scratch);
    const double *VpadPtr = nullptr;
    if (m == Interp2Method::Cubic) {
        if (R < 3 || C < 3)
            throw Error("interp2: 'cubic' requires at least 3 points in each dimension",
                         0, 0, "interp2", "", "numkit:interp2:cubicSize");
        auto isUniform = [](const double *g, std::size_t n) {
            if (n < 2) return true;
            const double step = g[1] - g[0];
            for (std::size_t i = 2; i < n; ++i)
                if (std::abs((g[i] - g[i - 1]) - step) > 1e-10 * std::max(1.0, std::abs(step)))
                    return false;
            return true;
        };
        if (!isUniform(xGrid, C) || !isUniform(yGrid, R))
            throw Error("interp2: 'cubic' requires a uniformly-spaced grid",
                         0, 0, "interp2", "", "numkit:interp2:cubicNonUniform");
        const std::size_t PR = R + 2, PC = C + 2;
        Vpad.assign(PR * PC, 0.0);
        auto at = [&](std::size_t i, std::size_t j) -> double & { return Vpad[j * PR + i]; };
        // Centre.
        for (std::size_t j = 0; j < C; ++j)
            for (std::size_t i = 0; i < R; ++i)
                at(i + 1, j + 1) = Vd[j * R + i];
        // Pad top/bottom rows across the original columns.
        for (std::size_t j = 0; j < C; ++j) {
            const double *col = &Vd[j * R];
            at(0,     j + 1) = 3.0 * col[0]     - 3.0 * col[1]     + col[2];
            at(R + 1, j + 1) = 3.0 * col[R - 1] - 3.0 * col[R - 2] + col[R - 3];
        }
        // Pad left/right columns across ALL padded rows (corners included).
        for (std::size_t i = 0; i < PR; ++i) {
            at(i, 0)     = 3.0 * at(i, 1)     - 3.0 * at(i, 2)     + at(i, 3);
            at(i, C + 1) = 3.0 * at(i, C)     - 3.0 * at(i, C - 1) + at(i, C - 2);
        }
        VpadPtr = Vpad.data();
    }

    // Separable 'spline': interpolate each grid row along x at xq, then
    // interpolate the resulting column along y at yq. The cubic spline is a
    // linear operator, so this sequential 1-D form equals the 2-D
    // tensor-product spline and matches MATLAB exactly — including
    // out-of-range extrapolation, non-uniform grids, and the <3-point
    // linear fallback, all inherited from the verified 1-D interpSpline.
    // A row-major copy of V makes per-row access contiguous.
    ScratchVec<double> Vrow(&scratch);
    if (m == Interp2Method::Spline) {
        Vrow.resize(R * C);
        for (std::size_t i = 0; i < R; ++i)
            for (std::size_t j = 0; j < C; ++j)
                Vrow[i * C + j] = Vd[j * R + i];
    }

    auto sampleAt = [&](double xq, double yq) -> double {
        if (m != Interp2Method::Spline)
            return interp2Sample(Vd.data(), R, C, xGrid, yGrid, xq, yq, m, VpadPtr);
        ScratchArena local(mr);
        ScratchVec<double> col(R, &local);
        for (std::size_t i = 0; i < R; ++i) {
            const double *rowData = &Vrow[i * C];
            auto v = interpSpline(xGrid, rowData, C, &xq, 1, &local);
            col[i] = v[0];
        }
        auto outv = interpSpline(yGrid, col.data(), R, &yq, 1, &local);
        return outv[0];
    };

    // MATLAB interp2 query semantics:
    //   • Xq and Yq the SAME size (incl. same-length vectors and meshgrid
    //     matrices), or either a scalar → POINTWISE: Vq(k) = sample(Xq(k),
    //     Yq(k)); the output has the shape of the non-scalar operand.
    //   • A row vector vs a column vector → implicit expansion to a grid:
    //     Vq is numel(Yq) × numel(Xq), Vq(i,j) = sample(Xq(j), Yq(i)).
    //   • Otherwise (different sizes, not broadcastable) → error, matching
    //     "Query coordinates input arrays must have the same size."
    const auto &xd = Xq.dims();
    const auto &yd = Yq.dims();
    const bool sameShape = (xd.rows() == yd.rows() && xd.cols() == yd.cols());
    const bool xqIsVec = xd.isVector() || Xq.isScalar();
    const bool yqIsVec = yd.isVector() || Yq.isScalar();

    if (sameShape || Xq.isScalar() || Yq.isScalar()) {
        // Pointwise (scalar operands broadcast against the other's shape).
        const Value &shapeRef = (Xq.numel() >= Yq.numel()) ? Xq : Yq;
        const std::size_t nq = shapeRef.numel();
        auto out = Value::matrix(shapeRef.dims().rows(), shapeRef.dims().cols(),
                                 ValueType::DOUBLE, mr);
        double *dst = out.doubleDataMut();
        for (std::size_t i = 0; i < nq; ++i) {
            const double xq = Xq.isScalar() ? Xq.elemAsDouble(0)
                                            : Xq.elemAsDouble(i);
            const double yq = Yq.isScalar() ? Yq.elemAsDouble(0)
                                            : Yq.elemAsDouble(i);
            dst[i] = sampleAt(xq, yq);
        }
        return out;
    }

    const bool xqRow = (xd.rows() == 1), xqCol = (xd.cols() == 1);
    const bool yqRow = (yd.rows() == 1), yqCol = (yd.cols() == 1);
    if (xqIsVec && yqIsVec && ((xqRow && yqCol) || (xqCol && yqRow))) {
        // Implicit expansion: Xq supplies the columns, Yq supplies the rows.
        const std::size_t nx = Xq.numel();
        const std::size_t ny = Yq.numel();
        auto out = Value::matrix(ny, nx, ValueType::DOUBLE, mr);
        double *dst = out.doubleDataMut();
        for (std::size_t j = 0; j < nx; ++j) {
            const double xq = Xq.elemAsDouble(j);
            for (std::size_t i = 0; i < ny; ++i)
                dst[j * ny + i] = sampleAt(xq, Yq.elemAsDouble(i));
        }
        return out;
    }

    throw Error("interp2: Xq and Yq must have the same size",
                 0, 0, "interp2", "", "numkit:interp2:queryShape");
}

} // namespace
namespace {

// Trilinear / nearest sample at (xq, yq, zq) given a 3D V (rows R,
// cols C, pages P) in column-major page-major layout: V[k*R*C + j*R + i]
// for (row=i, col=j, page=k).
double interp3Sample(const double *V, std::size_t R, std::size_t C, std::size_t P,
                     const double *xGrid, const double *yGrid, const double *zGrid,
                     double xq, double yq, double zq, Interp2Method method)
{
    const std::size_t ix = findCell(xGrid, C, xq);
    const std::size_t iy = findCell(yGrid, R, yq);
    const std::size_t iz = findCell(zGrid, P, zq);
    if (ix == std::size_t(-1) || iy == std::size_t(-1) || iz == std::size_t(-1))
        return std::nan("");

    auto val = [&](std::size_t i, std::size_t j, std::size_t k) {
        return V[k * R * C + j * R + i];
    };

    if (method == Interp2Method::Nearest) {
        const std::size_t cx = (xq - xGrid[ix] <= xGrid[ix + 1] - xq) ? ix : ix + 1;
        const std::size_t cy = (yq - yGrid[iy] <= yGrid[iy + 1] - yq) ? iy : iy + 1;
        const std::size_t cz = (zq - zGrid[iz] <= zGrid[iz + 1] - zq) ? iz : iz + 1;
        return val(cy, cx, cz);
    }
    const double tx = (xq - xGrid[ix]) / (xGrid[ix + 1] - xGrid[ix]);
    const double ty = (yq - yGrid[iy]) / (yGrid[iy + 1] - yGrid[iy]);
    const double tz = (zq - zGrid[iz]) / (zGrid[iz + 1] - zGrid[iz]);
    // Eight corners. (i, j, k) = (row, col, page) in V's index space.
    const double v000 = val(iy,     ix,     iz    );
    const double v100 = val(iy,     ix + 1, iz    );
    const double v010 = val(iy + 1, ix,     iz    );
    const double v110 = val(iy + 1, ix + 1, iz    );
    const double v001 = val(iy,     ix,     iz + 1);
    const double v101 = val(iy,     ix + 1, iz + 1);
    const double v011 = val(iy + 1, ix,     iz + 1);
    const double v111 = val(iy + 1, ix + 1, iz + 1);
    const double c00 = (1 - tx) * v000 + tx * v100;
    const double c10 = (1 - tx) * v010 + tx * v110;
    const double c01 = (1 - tx) * v001 + tx * v101;
    const double c11 = (1 - tx) * v011 + tx * v111;
    const double c0  = (1 - ty) * c00 + ty * c10;
    const double c1  = (1 - ty) * c01 + ty * c11;
    return (1 - tz) * c0 + tz * c1;
}

Value interp3Impl(const Value &V, const double *xGrid, std::size_t xN, const double *yGrid, std::size_t yN, const double *zGrid, std::size_t zN, const Value &Xq, const Value &Yq, const Value &Zq, const std::string &method, std::pmr::memory_resource *mr)
{
    if (V.type() == ValueType::COMPLEX)
        throw Error("interp3: complex inputs are not supported",
                     0, 0, "interp3", "", "numkit:interp3:complex");
    if (!V.dims().is3D())
        throw Error("interp3: V must be a 3D array",
                     0, 0, "interp3", "", "numkit:interp3:rank");
    if (Xq.numel() != Yq.numel() || Xq.numel() != Zq.numel())
        throw Error("interp3: Xq, Yq, Zq must have the same numel",
                     0, 0, "interp3", "", "numkit:interp3:queryShape");

    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    const std::size_t P = V.dims().pages();
    if (xN != C || yN != R || zN != P)
        throw Error("interp3: grid lengths must equal V's dim sizes",
                     0, 0, "interp3", "", "numkit:interp3:gridSize");
    validateMonotonicAscending(xGrid, C, "X");
    validateMonotonicAscending(yGrid, R, "Y");
    validateMonotonicAscending(zGrid, P, "Z");

    const Interp2Method m = parseInterp2Method(method);
    ScratchArena scratch(mr);
    ScratchVec<double> Vd(R * C * P, &scratch);
    if (V.type() == ValueType::DOUBLE)
        std::memcpy(Vd.data(), V.doubleData(), R * C * P * sizeof(double));
    else
        for (std::size_t i = 0; i < R * C * P; ++i) Vd[i] = V.elemAsDouble(i);

    const auto &qd = Xq.dims();
    const std::size_t nq = Xq.numel();
    auto out = Value::matrix(qd.rows(), qd.cols(), ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < nq; ++i) {
        const double xq = Xq.elemAsDouble(i);
        const double yq = Yq.elemAsDouble(i);
        const double zq = Zq.elemAsDouble(i);
        dst[i] = interp3Sample(Vd.data(), R, C, P,
                               xGrid, yGrid, zGrid,
                               xq, yq, zq, m);
    }
    return out;
}

} // namespace

} // namespace numkit::builtin
