// libs/builtin/src/math/interpolation/interp.cpp
//
// 1-D / 2-D / 3-D interpolation. polyfit / polyval moved to
// math/elementary/polynomials.cpp; trapz to math/integration/integration.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/math/interp/interp.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory_resource>
#include <string>

namespace numkit::builtin {

// ── Internal algorithm helpers ────────────────────────────────────────

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
interpLinear(std::pmr::memory_resource *mr,
             const double *x, const double *y, size_t n,
             const double *xq, size_t nq)
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
interpNearest(std::pmr::memory_resource *mr,
              const double *x, const double *y, size_t n,
              const double *xq, size_t nq)
{
    ScratchVec<double> yq(nq, mr);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);
        if (std::abs(xq[k] - x[i]) <= std::abs(xq[k] - x[i + 1]))
            yq[k] = y[i];
        else
            yq[k] = y[i + 1];
    }
    return yq;
}

// Build the n-vector of second derivatives at knots (sigma) using
// MATLAB's not-a-knot boundary conditions. Falls back to natural BCs
// for n == 3 (NaK is degenerate). Tridiagonal system has the standard
// interior rows (j=1..n-2) with the first/last rows modified to
// substitute sigma_0 / sigma_{n-1} with their NaK linear combinations.
ScratchVec<double>
computeSplineSigma(std::pmr::memory_resource *mr,
                   const double *x, const double *y, size_t n,
                   const double *h)
{
    ScratchVec<double> sigma(n, 0.0, mr);
    if (n < 3) return sigma;

    const size_t m = n - 2;
    ScratchVec<double> diag(m, mr), upper(m, mr), lower(m, mr), rhs(m, mr);

    for (size_t i = 0; i < m; ++i) {
        const size_t j = i + 1;
        diag[i] = 2.0 * (h[j - 1] + h[j]);
        rhs[i] = 6.0 * ((y[j + 1] - y[j]) / h[j] - (y[j] - y[j - 1]) / h[j - 1]);
        if (i > 0)         lower[i] = h[j - 1];
        if (i < m - 1)     upper[i] = h[j];
    }

    // Not-a-knot modifications (only when n >= 4; for n==3 fall back
    // to natural BCs). See BUGS.md note attached to spline parity:
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
interpSpline(std::pmr::memory_resource *mr,
             const double *x, const double *y, size_t n,
             const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(mr, x, y, n, xq, nq);

    const size_t nm1 = n - 1;

    ScratchVec<double> h(nm1, mr);
    for (size_t i = 0; i < nm1; ++i)
        h[i] = x[i + 1] - x[i];

    auto sigma = computeSplineSigma(mr, x, y, n, h.data());

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
interpPchip(std::pmr::memory_resource *mr,
            const double *x, const double *y, size_t n,
            const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(mr, x, y, n, xq, nq);

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

} // anonymous namespace

// ── interp1 ───────────────────────────────────────────────────────────
Value interp1(std::pmr::memory_resource *mr,
               const Value &x,
               const Value &y,
               const Value &xq,
               const std::string &method)
{
    const size_t n = x.numel();
    const size_t nq = xq.numel();

    if (n != y.numel())
        throw Error("interp1: x and y must have same length",
                     0, 0, "interp1", "", "m:interp1:lengthMismatch");
    if (n < 2)
        throw Error("interp1: need at least 2 data points",
                     0, 0, "interp1", "", "m:interp1:tooFewPoints");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    const double *xqd = xq.doubleData();

    ScratchArena scratch(mr);

    if (method == "linear") {
        auto yq = interpLinear(&scratch, xd, yd, n, xqd, nq);
        return packInterpResult(yq.data(), yq.size(), xq, mr);
    }
    if (method == "nearest") {
        auto yq = interpNearest(&scratch, xd, yd, n, xqd, nq);
        return packInterpResult(yq.data(), yq.size(), xq, mr);
    }
    if (method == "spline") {
        auto yq = interpSpline(&scratch, xd, yd, n, xqd, nq);
        return packInterpResult(yq.data(), yq.size(), xq, mr);
    }
    if (method == "pchip") {
        auto yq = interpPchip(&scratch, xd, yd, n, xqd, nq);
        return packInterpResult(yq.data(), yq.size(), xq, mr);
    }
    throw Error("interp1: unknown method '" + method + "'",
                 0, 0, "interp1", "", "m:interp1:badMethod");
}

// ── interp2 ───────────────────────────────────────────────────────────
namespace {

enum class Interp2Method { Linear, Nearest };

Interp2Method parseInterp2Method(const std::string &m)
{
    std::string s = m;
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s.empty() || s == "linear") return Interp2Method::Linear;
    if (s == "nearest")             return Interp2Method::Nearest;
    if (s == "spline" || s == "cubic" || s == "pchip")
        throw Error("interp2: '" + m + "' method not yet supported "
                     "(only 'linear' and 'nearest' for now)",
                     0, 0, "interp2", "", "m:interp2:unsupportedMethod");
    throw Error("interp2: unknown method '" + m + "'",
                 0, 0, "interp2", "", "m:interp2:badMethod");
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
                         0, 0, "interp2", "", "m:interp2:notMonotonic");
}

// Fast path: V is column-major (rows = R, cols = C). Sample one bilinear
// or nearest-neighbour value at (xq, yq) using x grid (length C) and y
// grid (length R).
double interp2Sample(const double *V, std::size_t R, std::size_t C,
                     const double *xGrid, const double *yGrid,
                     double xq, double yq, Interp2Method method)
{
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

void readGridAxis(const Value &g, ScratchVec<double> &out, const char *axis)
{
    if (!g.dims().isVector() && !g.isScalar())
        throw Error(std::string("interp2: ") + axis
                     + " must be a vector",
                     0, 0, "interp2", "", "m:interp2:notVector");
    out.resize(g.numel());
    for (std::size_t i = 0; i < g.numel(); ++i) out[i] = g.elemAsDouble(i);
}

Value interp2Impl(std::pmr::memory_resource *mr,
                   const Value &V,
                   const double *xGrid, std::size_t xN,
                   const double *yGrid, std::size_t yN,
                   const Value &Xq, const Value &Yq,
                   const std::string &method)
{
    if (V.type() == ValueType::COMPLEX)
        throw Error("interp2: complex inputs are not supported",
                     0, 0, "interp2", "", "m:interp2:complex");
    if (V.dims().is3D() || V.dims().ndim() > 2)
        throw Error("interp2: V must be a 2D matrix",
                     0, 0, "interp2", "", "m:interp2:rank");
    if (Xq.numel() != Yq.numel())
        throw Error("interp2: Xq and Yq must have the same numel",
                     0, 0, "interp2", "", "m:interp2:queryShape");

    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    if (xN != C)
        throw Error("interp2: length(X) must equal cols(V)",
                     0, 0, "interp2", "", "m:interp2:gridSize");
    if (yN != R)
        throw Error("interp2: length(Y) must equal rows(V)",
                     0, 0, "interp2", "", "m:interp2:gridSize");
    validateMonotonicAscending(xGrid, C, "X");
    validateMonotonicAscending(yGrid, R, "Y");

    const Interp2Method m = parseInterp2Method(method);
    ScratchArena scratch(mr);
    // V as DOUBLE (promote if needed).
    ScratchVec<double> Vd(R * C, &scratch);
    if (V.type() == ValueType::DOUBLE)
        std::memcpy(Vd.data(), V.doubleData(), R * C * sizeof(double));
    else
        for (std::size_t i = 0; i < R * C; ++i) Vd[i] = V.elemAsDouble(i);

    // Output shape: take Xq's shape (or rather build same-shape result).
    const auto &qd = Xq.dims();
    const std::size_t nq = Xq.numel();
    auto out = Value::matrix(qd.rows(), qd.cols(), ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < nq; ++i) {
        const double xq = Xq.elemAsDouble(i);
        const double yq = Yq.elemAsDouble(i);
        dst[i] = interp2Sample(Vd.data(), R, C, xGrid, yGrid, xq, yq, m);
    }
    return out;
}

} // namespace

Value interp2(std::pmr::memory_resource *mr, const Value &V,
               const Value &Xq, const Value &Yq, const std::string &method)
{
    if (V.dims().is3D() || V.dims().ndim() > 2)
        throw Error("interp2: V must be a 2D matrix",
                     0, 0, "interp2", "", "m:interp2:rank");
    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    ScratchArena scratch(mr);
    auto xGrid = ScratchVec<double>(C, &scratch);
    auto yGrid = ScratchVec<double>(R, &scratch);
    for (std::size_t i = 0; i < C; ++i) xGrid[i] = static_cast<double>(i + 1);
    for (std::size_t i = 0; i < R; ++i) yGrid[i] = static_cast<double>(i + 1);
    return interp2Impl(mr, V, xGrid.data(), xGrid.size(),
                       yGrid.data(), yGrid.size(), Xq, Yq, method);
}

Value interp2(std::pmr::memory_resource *mr, const Value &X, const Value &Y,
               const Value &V, const Value &Xq, const Value &Yq,
               const std::string &method)
{
    ScratchArena scratch(mr);
    ScratchVec<double> xGrid(&scratch), yGrid(&scratch);
    readGridAxis(X, xGrid, "X");
    readGridAxis(Y, yGrid, "Y");
    return interp2Impl(mr, V, xGrid.data(), xGrid.size(),
                       yGrid.data(), yGrid.size(), Xq, Yq, method);
}

// ── interp3 ───────────────────────────────────────────────────────────
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

Value interp3Impl(std::pmr::memory_resource *mr,
                   const Value &V,
                   const double *xGrid, std::size_t xN,
                   const double *yGrid, std::size_t yN,
                   const double *zGrid, std::size_t zN,
                   const Value &Xq, const Value &Yq, const Value &Zq,
                   const std::string &method)
{
    if (V.type() == ValueType::COMPLEX)
        throw Error("interp3: complex inputs are not supported",
                     0, 0, "interp3", "", "m:interp3:complex");
    if (!V.dims().is3D())
        throw Error("interp3: V must be a 3D array",
                     0, 0, "interp3", "", "m:interp3:rank");
    if (Xq.numel() != Yq.numel() || Xq.numel() != Zq.numel())
        throw Error("interp3: Xq, Yq, Zq must have the same numel",
                     0, 0, "interp3", "", "m:interp3:queryShape");

    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    const std::size_t P = V.dims().pages();
    if (xN != C || yN != R || zN != P)
        throw Error("interp3: grid lengths must equal V's dim sizes",
                     0, 0, "interp3", "", "m:interp3:gridSize");
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

Value interp3(std::pmr::memory_resource *mr, const Value &V,
               const Value &Xq, const Value &Yq, const Value &Zq,
               const std::string &method)
{
    if (!V.dims().is3D())
        throw Error("interp3: V must be a 3D array",
                     0, 0, "interp3", "", "m:interp3:rank");
    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    const std::size_t P = V.dims().pages();
    ScratchArena scratch(mr);
    auto xGrid = ScratchVec<double>(C, &scratch);
    auto yGrid = ScratchVec<double>(R, &scratch);
    auto zGrid = ScratchVec<double>(P, &scratch);
    for (std::size_t i = 0; i < C; ++i) xGrid[i] = static_cast<double>(i + 1);
    for (std::size_t i = 0; i < R; ++i) yGrid[i] = static_cast<double>(i + 1);
    for (std::size_t i = 0; i < P; ++i) zGrid[i] = static_cast<double>(i + 1);
    return interp3Impl(mr, V, xGrid.data(), xGrid.size(),
                       yGrid.data(), yGrid.size(),
                       zGrid.data(), zGrid.size(), Xq, Yq, Zq, method);
}

Value interp3(std::pmr::memory_resource *mr, const Value &X, const Value &Y, const Value &Z,
               const Value &V, const Value &Xq, const Value &Yq, const Value &Zq,
               const std::string &method)
{
    ScratchArena scratch(mr);
    ScratchVec<double> xGrid(&scratch), yGrid(&scratch),
                        zGrid(&scratch);
    readGridAxis(X, xGrid, "X");
    readGridAxis(Y, yGrid, "Y");
    readGridAxis(Z, zGrid, "Z");
    return interp3Impl(mr, V, xGrid.data(), xGrid.size(),
                       yGrid.data(), yGrid.size(),
                       zGrid.data(), zGrid.size(), Xq, Yq, Zq, method);
}

// ── spline ────────────────────────────────────────────────────────────
Value spline(std::pmr::memory_resource *mr, const Value &x, const Value &y, const Value &xq)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw Error("spline: x and y must have same length",
                     0, 0, "spline", "", "m:spline:lengthMismatch");
    if (n < 2)
        throw Error("spline: need at least 2 data points",
                     0, 0, "spline", "", "m:spline:tooFewPoints");

    ScratchArena scratch(mr);
    auto yq = interpSpline(&scratch,
                           x.doubleData(), y.doubleData(), n,
                           xq.doubleData(), xq.numel());
    return packInterpResult(yq.data(), yq.size(), xq, mr);
}

// ── pchip ─────────────────────────────────────────────────────────────
Value pchip(std::pmr::memory_resource *mr, const Value &x, const Value &y, const Value &xq)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw Error("pchip: x and y must have same length",
                     0, 0, "pchip", "", "m:pchip:lengthMismatch");
    if (n < 2)
        throw Error("pchip: need at least 2 data points",
                     0, 0, "pchip", "", "m:pchip:tooFewPoints");

    ScratchArena scratch(mr);
    auto yq = interpPchip(&scratch,
                          x.doubleData(), y.doubleData(), n,
                          xq.doubleData(), xq.numel());
    return packInterpResult(yq.data(), yq.size(), xq, mr);
}

// polyfit / polyval moved to math/elementary/polynomials.cpp
// trapz moved to math/integration/integration.cpp

// ── Pack 30: mkpp / ppval ────────────────────────────────────────────

Value mkpp(std::pmr::memory_resource *mr, const Value &breaks, const Value &coefs)
{
    if (breaks.numel() < 2)
        throw Error("mkpp: breaks must have at least 2 entries",
                     0, 0, "mkpp", "", "m:mkpp:breaks");
    const size_t L = breaks.numel() - 1;  // pieces
    if (coefs.dims().ndim() > 2)
        throw Error("mkpp: only 2-D coefs (pieces × order) supported",
                     0, 0, "mkpp", "", "m:mkpp:rank");
    const size_t pieces = coefs.dims().rows();
    const size_t order  = coefs.dims().cols();
    if (pieces != L)
        throw Error("mkpp: rows(coefs) must equal numel(breaks) - 1",
                     0, 0, "mkpp", "", "m:mkpp:shape");

    auto pp = Value::structure(mr);
    pp.field("form")   = Value::fromString("pp", mr);
    pp.field("breaks") = breaks;
    pp.field("coefs")  = coefs;
    pp.field("pieces") = Value::scalar(static_cast<double>(L), mr);
    pp.field("order")  = Value::scalar(static_cast<double>(order), mr);
    pp.field("dim")    = Value::scalar(1.0, mr);
    return pp;
}

Value ppval(std::pmr::memory_resource *mr, const Value &pp, const Value &x)
{
    if (!pp.isStruct() || !pp.hasField("breaks") || !pp.hasField("coefs"))
        throw Error("ppval: first argument must be a pp struct",
                     0, 0, "ppval", "", "m:ppval:notPp");
    const Value &breaks = pp.field("breaks");
    const Value &coefs  = pp.field("coefs");
    const size_t L      = breaks.numel() - 1;
    const size_t order  = coefs.dims().cols();
    const double *bp    = breaks.doubleData();
    const double *cp    = coefs.doubleData();
    const size_t cR     = coefs.dims().rows();   // pieces (column-major stride)

    auto out = createLike(x, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t N = x.numel();
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        // Find piece index j such that breaks[j] <= xi < breaks[j+1].
        // Linear scan is fine for typical L ≤ ~64; binary search if it
        // ever matters.
        size_t j = 0;
        if (xi <= bp[0]) j = 0;
        else if (xi >= bp[L]) j = L - 1;
        else {
            // First k with bp[k+1] > xi.
            j = L - 1;
            for (size_t k = 0; k < L; ++k) {
                if (xi < bp[k + 1]) { j = k; break; }
            }
        }
        const double u = xi - bp[j];
        // Local polynomial: coefs(j, 0) is the leading (highest) power.
        double y = cp[0 * cR + j];
        for (size_t k = 1; k < order; ++k)
            y = y * u + cp[k * cR + j];
        dst[i] = y;
    }
    return out;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void interp1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("interp1: requires at least 3 arguments",
                     0, 0, "interp1", "", "m:interp1:nargin");
    std::string method = "linear";
    if (args.size() >= 4 && args[3].isChar())
        method = args[3].toString();
    outs[0] = interp1(ctx.engine->resource(), args[0], args[1], args[2], method);
}

// 2-arg `spline(x, y)` returns a pp struct (piecewise polynomial form)
// usable with `ppval`. Coefficients are derived from the natural cubic
// spline's second-derivative form via the standard transformation
// (see comment block in implementation). See BUGS.md #22.
namespace {

Value splinePp(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw Error("spline: x and y must have same length",
                     0, 0, "spline", "", "m:spline:lengthMismatch");
    if (n < 2)
        throw Error("spline: need at least 2 data points",
                     0, 0, "spline", "", "m:spline:tooFewPoints");

    ScratchArena scratch(mr);
    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    // Reuse interpSpline's not-a-knot sigma helper. See BUGS.md #22.
    const size_t nm1 = n - 1;
    ScratchVec<double> h(nm1, &scratch);
    for (size_t i = 0; i < nm1; ++i) h[i] = xd[i + 1] - xd[i];
    auto sigma = computeSplineSigma(&scratch, xd, yd, n, h.data());

    // Build [nm1 x 4] coefficient matrix in column-major order.
    // For each interval i, with dx = x - xd[i] in [0, h_i]:
    //   y(dx) = a*dx^3 + b*dx^2 + c*dx + d
    //   a = (sigma_{i+1} - sigma_i) / (6 * h_i)
    //   b = sigma_i / 2
    //   c = (y_{i+1} - y_i) / h_i - h_i * (2*sigma_i + sigma_{i+1}) / 6
    //   d = y_i
    auto coefs = Value::matrix(nm1, 4, ValueType::DOUBLE, mr);
    double *cp = coefs.doubleDataMut();
    for (size_t i = 0; i < nm1; ++i) {
        const double hi = h[i];
        const double a  = (sigma[i + 1] - sigma[i]) / (6.0 * hi);
        const double b  = sigma[i] / 2.0;
        const double c  = (yd[i + 1] - yd[i]) / hi
                          - hi * (2.0 * sigma[i] + sigma[i + 1]) / 6.0;
        const double d  = yd[i];
        cp[i + 0 * nm1] = a;   // col 0
        cp[i + 1 * nm1] = b;
        cp[i + 2 * nm1] = c;
        cp[i + 3 * nm1] = d;
    }
    return mkpp(mr, x, coefs);
}

} // namespace

void spline_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // pp-struct form. See BUGS.md #22.
        outs[0] = splinePp(mr, args[0], args[1]);
        return;
    }
    if (args.size() < 3)
        throw Error("spline: requires (x, y) or (x, y, xq)",
                     0, 0, "spline", "", "m:spline:nargin");
    outs[0] = spline(mr, args[0], args[1], args[2]);
}

void interp2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("interp2: requires at least 3 arguments",
                     0, 0, "interp2", "", "m:interp2:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    auto isMethodArg = [](const Value &v) {
        return v.isChar() || v.isString();
    };
    // Form A: interp2(V, Xq, Yq[, method]) — first arg is the matrix.
    // Form B: interp2(X, Y, V, Xq, Yq[, method]) — 5 or 6 numeric args.
    if (args.size() == 3 || (args.size() == 4 && isMethodArg(args[3]))) {
        std::string method = "linear";
        if (args.size() == 4) method = args[3].toString();
        outs[0] = interp2(mr, args[0], args[1], args[2], method);
        return;
    }
    if (args.size() == 5 || (args.size() == 6 && isMethodArg(args[5]))) {
        std::string method = "linear";
        if (args.size() == 6) method = args[5].toString();
        outs[0] = interp2(mr, args[0], args[1], args[2], args[3], args[4], method);
        return;
    }
    throw Error("interp2: invalid argument count or types",
                 0, 0, "interp2", "", "m:interp2:nargin");
}

void interp3_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("interp3: requires at least 4 arguments",
                     0, 0, "interp3", "", "m:interp3:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    auto isMethodArg = [](const Value &v) {
        return v.isChar() || v.isString();
    };
    // Form A: interp3(V, Xq, Yq, Zq[, method]).
    if (args.size() == 4 || (args.size() == 5 && isMethodArg(args[4]))) {
        std::string method = "linear";
        if (args.size() == 5) method = args[4].toString();
        outs[0] = interp3(mr, args[0], args[1], args[2], args[3], method);
        return;
    }
    // Form B: interp3(X, Y, Z, V, Xq, Yq, Zq[, method]) — 7 or 8 args.
    if (args.size() == 7 || (args.size() == 8 && isMethodArg(args[7]))) {
        std::string method = "linear";
        if (args.size() == 8) method = args[7].toString();
        outs[0] = interp3(mr, args[0], args[1], args[2], args[3],
                          args[4], args[5], args[6], method);
        return;
    }
    throw Error("interp3: invalid argument count or types",
                 0, 0, "interp3", "", "m:interp3:nargin");
}

void pchip_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("pchip: requires 3 arguments",
                     0, 0, "pchip", "", "m:pchip:nargin");
    outs[0] = pchip(ctx.engine->resource(), args[0], args[1], args[2]);
}

// interpn — dispatch to interp2 / interp3 based on V's ndim. Form A
// (V, Xq1..XqN[, method]) inspects args[0]; Form B
// (X1..XN, V, Xq1..XqN[, method]) follows the same dispatch pattern
// because V always lives at args[0] in Form A and the implementation
// distinguishes them inside interp2_reg / interp3_reg.
void interpn_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("interpn: requires at least 2 arguments",
                     0, 0, "interpn", "", "m:interpn:nargin");
    const auto &V0 = args[0];
    const int ndV = V0.dims().is3D() ? 3
                  : (V0.dims().ndim() <= 2 ? 2 : V0.dims().ndim());
    if (ndV == 2) {
        interp2_reg(args, nargout, outs, ctx);
        return;
    }
    if (ndV == 3) {
        interp3_reg(args, nargout, outs, ctx);
        return;
    }
    throw Error("interpn: 4+-D inputs are not yet supported",
                 0, 0, "interpn", "", "m:interpn:rank");
}

// polyfit_reg / polyval_reg → math/elementary/polynomials.cpp
// trapz_reg                 → math/integration/integration.cpp

void mkpp_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mkpp: requires (breaks, coefs)",
                     0, 0, "mkpp", "", "m:mkpp:nargin");
    outs[0] = mkpp(ctx.engine->resource(), args[0], args[1]);
}

void ppval_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ppval: requires (pp, x)",
                     0, 0, "ppval", "", "m:ppval:nargin");
    outs[0] = ppval(ctx.engine->resource(), args[0], args[1]);
}

void unmkpp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unmkpp: requires 1 argument",
                     0, 0, "unmkpp", "", "m:unmkpp:nargin");
    const Value &pp = args[0];
    if (!pp.isStruct() || !pp.hasField("breaks") || !pp.hasField("coefs"))
        throw Error("unmkpp: input must be a pp struct",
                     0, 0, "unmkpp", "", "m:unmkpp:notPp");
    outs[0] = pp.field("breaks");
    if (nargout > 1) outs[1] = pp.field("coefs");
    if (nargout > 2) outs[2] = pp.hasField("pieces") ? pp.field("pieces")
                                                     : Value::scalar(0.0, ctx.engine->resource());
    if (nargout > 3) outs[3] = pp.hasField("order")  ? pp.field("order")
                                                     : Value::scalar(0.0, ctx.engine->resource());
    if (nargout > 4) outs[4] = pp.hasField("dim")    ? pp.field("dim")
                                                     : Value::scalar(1.0, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
