// libs/builtin/src/math/interpolation/interp.cpp
//
// 1-D / 2-D / 3-D interpolation. polyfit / polyval moved to
// math/elementary/polynomials.cpp; trapz to math/integration/integration.cpp.

#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/math/interpolation/interp.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::m::builtin {

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

std::vector<double>
interpLinear(const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    std::vector<double> yq(nq);
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

std::vector<double>
interpNearest(const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        const size_t i = findInterval(x, n, xq[k]);
        if (std::abs(xq[k] - x[i]) <= std::abs(xq[k] - x[i + 1]))
            yq[k] = y[i];
        else
            yq[k] = y[i + 1];
    }
    return yq;
}

std::vector<double>
interpSpline(const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq);

    const size_t nm1 = n - 1;

    std::vector<double> h(nm1);
    for (size_t i = 0; i < nm1; ++i)
        h[i] = x[i + 1] - x[i];

    const size_t m = n - 2;
    std::vector<double> sigma(n, 0.0);

    if (m > 0) {
        std::vector<double> diag(m), upper(m), lower(m), rhs(m);

        for (size_t i = 0; i < m; ++i) {
            const size_t j = i + 1;
            diag[i] = 2.0 * (h[j - 1] + h[j]);
            rhs[i] = 6.0 * ((y[j + 1] - y[j]) / h[j] - (y[j] - y[j - 1]) / h[j - 1]);
            if (i > 0)
                lower[i] = h[j - 1];
            if (i < m - 1)
                upper[i] = h[j];
        }

        for (size_t i = 1; i < m; ++i) {
            const double w = lower[i] / diag[i - 1];
            diag[i] -= w * upper[i - 1];
            rhs[i] -= w * rhs[i - 1];
        }

        sigma[m] = rhs[m - 1] / diag[m - 1];
        for (int i = static_cast<int>(m) - 2; i >= 0; --i)
            sigma[i + 1] = (rhs[i] - upper[i] * sigma[i + 2]) / diag[i];
    }

    std::vector<double> yq(nq);
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

std::vector<double>
interpPchip(const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq);

    const size_t nm1 = n - 1;

    std::vector<double> h(nm1), delta(nm1);
    for (size_t i = 0; i < nm1; ++i) {
        h[i] = x[i + 1] - x[i];
        delta[i] = (y[i + 1] - y[i]) / h[i];
    }

    std::vector<double> d(n, 0.0);

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

    std::vector<double> yq(nq);
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

// Helper for interp1 / spline / pchip — pack yq vector into an MValue
// preserving xq's row/column orientation.
MValue packInterpResult(const std::vector<double> &yq, const MValue &xq, Allocator &alloc)
{
    const size_t nq = yq.size();
    const bool isRow = xq.dims().rows() == 1;
    auto r = isRow ? MValue::matrix(1, nq, MType::DOUBLE, &alloc)
                   : MValue::matrix(nq, 1, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nq; ++i)
        r.doubleDataMut()[i] = yq[i];
    return r;
}

} // anonymous namespace

// ── interp1 ───────────────────────────────────────────────────────────
MValue interp1(Allocator &alloc,
               const MValue &x,
               const MValue &y,
               const MValue &xq,
               const std::string &method)
{
    const size_t n = x.numel();
    const size_t nq = xq.numel();

    if (n != y.numel())
        throw MError("interp1: x and y must have same length",
                     0, 0, "interp1", "", "m:interp1:lengthMismatch");
    if (n < 2)
        throw MError("interp1: need at least 2 data points",
                     0, 0, "interp1", "", "m:interp1:tooFewPoints");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    const double *xqd = xq.doubleData();

    std::vector<double> yq;
    if (method == "linear")
        yq = interpLinear(xd, yd, n, xqd, nq);
    else if (method == "nearest")
        yq = interpNearest(xd, yd, n, xqd, nq);
    else if (method == "spline")
        yq = interpSpline(xd, yd, n, xqd, nq);
    else if (method == "pchip")
        yq = interpPchip(xd, yd, n, xqd, nq);
    else
        throw MError("interp1: unknown method '" + method + "'",
                     0, 0, "interp1", "", "m:interp1:badMethod");

    return packInterpResult(yq, xq, alloc);
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
        throw MError("interp2: '" + m + "' method not yet supported "
                     "(only 'linear' and 'nearest' for now)",
                     0, 0, "interp2", "", "m:interp2:unsupportedMethod");
    throw MError("interp2: unknown method '" + m + "'",
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
            throw MError(std::string("interp2: ") + axis
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

void readGridAxis(const MValue &g, std::vector<double> &out, const char *axis)
{
    if (!g.dims().isVector() && !g.isScalar())
        throw MError(std::string("interp2: ") + axis
                     + " must be a vector",
                     0, 0, "interp2", "", "m:interp2:notVector");
    out.resize(g.numel());
    for (std::size_t i = 0; i < g.numel(); ++i) out[i] = g.elemAsDouble(i);
}

MValue interp2Impl(Allocator &alloc, const MValue &V,
                   const std::vector<double> &xGrid,
                   const std::vector<double> &yGrid,
                   const MValue &Xq, const MValue &Yq,
                   const std::string &method)
{
    if (V.type() == MType::COMPLEX)
        throw MError("interp2: complex inputs are not supported",
                     0, 0, "interp2", "", "m:interp2:complex");
    if (V.dims().is3D() || V.dims().ndim() > 2)
        throw MError("interp2: V must be a 2D matrix",
                     0, 0, "interp2", "", "m:interp2:rank");
    if (Xq.numel() != Yq.numel())
        throw MError("interp2: Xq and Yq must have the same numel",
                     0, 0, "interp2", "", "m:interp2:queryShape");

    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    if (xGrid.size() != C)
        throw MError("interp2: length(X) must equal cols(V)",
                     0, 0, "interp2", "", "m:interp2:gridSize");
    if (yGrid.size() != R)
        throw MError("interp2: length(Y) must equal rows(V)",
                     0, 0, "interp2", "", "m:interp2:gridSize");
    validateMonotonicAscending(xGrid.data(), C, "X");
    validateMonotonicAscending(yGrid.data(), R, "Y");

    const Interp2Method m = parseInterp2Method(method);
    // V as DOUBLE (promote if needed).
    std::vector<double> Vd(R * C);
    if (V.type() == MType::DOUBLE)
        std::memcpy(Vd.data(), V.doubleData(), R * C * sizeof(double));
    else
        for (std::size_t i = 0; i < R * C; ++i) Vd[i] = V.elemAsDouble(i);

    // Output shape: take Xq's shape (or rather build same-shape result).
    const auto &qd = Xq.dims();
    const std::size_t nq = Xq.numel();
    auto out = MValue::matrix(qd.rows(), qd.cols(), MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < nq; ++i) {
        const double xq = Xq.elemAsDouble(i);
        const double yq = Yq.elemAsDouble(i);
        dst[i] = interp2Sample(Vd.data(), R, C, xGrid.data(), yGrid.data(),
                               xq, yq, m);
    }
    return out;
}

} // namespace

MValue interp2(Allocator &alloc, const MValue &V,
               const MValue &Xq, const MValue &Yq, const std::string &method)
{
    if (V.dims().is3D() || V.dims().ndim() > 2)
        throw MError("interp2: V must be a 2D matrix",
                     0, 0, "interp2", "", "m:interp2:rank");
    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    std::vector<double> xGrid(C), yGrid(R);
    for (std::size_t i = 0; i < C; ++i) xGrid[i] = static_cast<double>(i + 1);
    for (std::size_t i = 0; i < R; ++i) yGrid[i] = static_cast<double>(i + 1);
    return interp2Impl(alloc, V, xGrid, yGrid, Xq, Yq, method);
}

MValue interp2(Allocator &alloc, const MValue &X, const MValue &Y,
               const MValue &V, const MValue &Xq, const MValue &Yq,
               const std::string &method)
{
    std::vector<double> xGrid, yGrid;
    readGridAxis(X, xGrid, "X");
    readGridAxis(Y, yGrid, "Y");
    return interp2Impl(alloc, V, xGrid, yGrid, Xq, Yq, method);
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

MValue interp3Impl(Allocator &alloc, const MValue &V,
                   const std::vector<double> &xGrid,
                   const std::vector<double> &yGrid,
                   const std::vector<double> &zGrid,
                   const MValue &Xq, const MValue &Yq, const MValue &Zq,
                   const std::string &method)
{
    if (V.type() == MType::COMPLEX)
        throw MError("interp3: complex inputs are not supported",
                     0, 0, "interp3", "", "m:interp3:complex");
    if (!V.dims().is3D())
        throw MError("interp3: V must be a 3D array",
                     0, 0, "interp3", "", "m:interp3:rank");
    if (Xq.numel() != Yq.numel() || Xq.numel() != Zq.numel())
        throw MError("interp3: Xq, Yq, Zq must have the same numel",
                     0, 0, "interp3", "", "m:interp3:queryShape");

    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    const std::size_t P = V.dims().pages();
    if (xGrid.size() != C || yGrid.size() != R || zGrid.size() != P)
        throw MError("interp3: grid lengths must equal V's dim sizes",
                     0, 0, "interp3", "", "m:interp3:gridSize");
    validateMonotonicAscending(xGrid.data(), C, "X");
    validateMonotonicAscending(yGrid.data(), R, "Y");
    validateMonotonicAscending(zGrid.data(), P, "Z");

    const Interp2Method m = parseInterp2Method(method);
    std::vector<double> Vd(R * C * P);
    if (V.type() == MType::DOUBLE)
        std::memcpy(Vd.data(), V.doubleData(), R * C * P * sizeof(double));
    else
        for (std::size_t i = 0; i < R * C * P; ++i) Vd[i] = V.elemAsDouble(i);

    const auto &qd = Xq.dims();
    const std::size_t nq = Xq.numel();
    auto out = MValue::matrix(qd.rows(), qd.cols(), MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < nq; ++i) {
        const double xq = Xq.elemAsDouble(i);
        const double yq = Yq.elemAsDouble(i);
        const double zq = Zq.elemAsDouble(i);
        dst[i] = interp3Sample(Vd.data(), R, C, P,
                               xGrid.data(), yGrid.data(), zGrid.data(),
                               xq, yq, zq, m);
    }
    return out;
}

} // namespace

MValue interp3(Allocator &alloc, const MValue &V,
               const MValue &Xq, const MValue &Yq, const MValue &Zq,
               const std::string &method)
{
    if (!V.dims().is3D())
        throw MError("interp3: V must be a 3D array",
                     0, 0, "interp3", "", "m:interp3:rank");
    const std::size_t R = V.dims().rows();
    const std::size_t C = V.dims().cols();
    const std::size_t P = V.dims().pages();
    std::vector<double> xGrid(C), yGrid(R), zGrid(P);
    for (std::size_t i = 0; i < C; ++i) xGrid[i] = static_cast<double>(i + 1);
    for (std::size_t i = 0; i < R; ++i) yGrid[i] = static_cast<double>(i + 1);
    for (std::size_t i = 0; i < P; ++i) zGrid[i] = static_cast<double>(i + 1);
    return interp3Impl(alloc, V, xGrid, yGrid, zGrid, Xq, Yq, Zq, method);
}

MValue interp3(Allocator &alloc, const MValue &X, const MValue &Y, const MValue &Z,
               const MValue &V, const MValue &Xq, const MValue &Yq, const MValue &Zq,
               const std::string &method)
{
    std::vector<double> xGrid, yGrid, zGrid;
    readGridAxis(X, xGrid, "X");
    readGridAxis(Y, yGrid, "Y");
    readGridAxis(Z, zGrid, "Z");
    return interp3Impl(alloc, V, xGrid, yGrid, zGrid, Xq, Yq, Zq, method);
}

// ── spline ────────────────────────────────────────────────────────────
MValue spline(Allocator &alloc, const MValue &x, const MValue &y, const MValue &xq)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw MError("spline: x and y must have same length",
                     0, 0, "spline", "", "m:spline:lengthMismatch");
    if (n < 2)
        throw MError("spline: need at least 2 data points",
                     0, 0, "spline", "", "m:spline:tooFewPoints");

    auto yq = interpSpline(x.doubleData(), y.doubleData(), n, xq.doubleData(), xq.numel());
    return packInterpResult(yq, xq, alloc);
}

// ── pchip ─────────────────────────────────────────────────────────────
MValue pchip(Allocator &alloc, const MValue &x, const MValue &y, const MValue &xq)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw MError("pchip: x and y must have same length",
                     0, 0, "pchip", "", "m:pchip:lengthMismatch");
    if (n < 2)
        throw MError("pchip: need at least 2 data points",
                     0, 0, "pchip", "", "m:pchip:tooFewPoints");

    auto yq = interpPchip(x.doubleData(), y.doubleData(), n, xq.doubleData(), xq.numel());
    return packInterpResult(yq, xq, alloc);
}

// polyfit / polyval moved to math/elementary/polynomials.cpp
// trapz moved to math/integration/integration.cpp

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void interp1_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("interp1: requires at least 3 arguments",
                     0, 0, "interp1", "", "m:interp1:nargin");
    std::string method = "linear";
    if (args.size() >= 4 && args[3].isChar())
        method = args[3].toString();
    outs[0] = interp1(ctx.engine->allocator(), args[0], args[1], args[2], method);
}

void spline_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("spline: requires 3 arguments",
                     0, 0, "spline", "", "m:spline:nargin");
    outs[0] = spline(ctx.engine->allocator(), args[0], args[1], args[2]);
}

void interp2_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("interp2: requires at least 3 arguments",
                     0, 0, "interp2", "", "m:interp2:nargin");
    Allocator &alloc = ctx.engine->allocator();
    auto isMethodArg = [](const MValue &v) {
        return v.isChar() || v.isString();
    };
    // Form A: interp2(V, Xq, Yq[, method]) — first arg is the matrix.
    // Form B: interp2(X, Y, V, Xq, Yq[, method]) — 5 or 6 numeric args.
    if (args.size() == 3 || (args.size() == 4 && isMethodArg(args[3]))) {
        std::string method = "linear";
        if (args.size() == 4) method = args[3].toString();
        outs[0] = interp2(alloc, args[0], args[1], args[2], method);
        return;
    }
    if (args.size() == 5 || (args.size() == 6 && isMethodArg(args[5]))) {
        std::string method = "linear";
        if (args.size() == 6) method = args[5].toString();
        outs[0] = interp2(alloc, args[0], args[1], args[2], args[3], args[4], method);
        return;
    }
    throw MError("interp2: invalid argument count or types",
                 0, 0, "interp2", "", "m:interp2:nargin");
}

void interp3_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw MError("interp3: requires at least 4 arguments",
                     0, 0, "interp3", "", "m:interp3:nargin");
    Allocator &alloc = ctx.engine->allocator();
    auto isMethodArg = [](const MValue &v) {
        return v.isChar() || v.isString();
    };
    // Form A: interp3(V, Xq, Yq, Zq[, method]).
    if (args.size() == 4 || (args.size() == 5 && isMethodArg(args[4]))) {
        std::string method = "linear";
        if (args.size() == 5) method = args[4].toString();
        outs[0] = interp3(alloc, args[0], args[1], args[2], args[3], method);
        return;
    }
    // Form B: interp3(X, Y, Z, V, Xq, Yq, Zq[, method]) — 7 or 8 args.
    if (args.size() == 7 || (args.size() == 8 && isMethodArg(args[7]))) {
        std::string method = "linear";
        if (args.size() == 8) method = args[7].toString();
        outs[0] = interp3(alloc, args[0], args[1], args[2], args[3],
                          args[4], args[5], args[6], method);
        return;
    }
    throw MError("interp3: invalid argument count or types",
                 0, 0, "interp3", "", "m:interp3:nargin");
}

void pchip_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("pchip: requires 3 arguments",
                     0, 0, "pchip", "", "m:pchip:nargin");
    outs[0] = pchip(ctx.engine->allocator(), args[0], args[1], args[2]);
}

// interpn — dispatch to interp2 / interp3 based on V's ndim. Form A
// (V, Xq1..XqN[, method]) inspects args[0]; Form B
// (X1..XN, V, Xq1..XqN[, method]) follows the same dispatch pattern
// because V always lives at args[0] in Form A and the implementation
// distinguishes them inside interp2_reg / interp3_reg.
void interpn_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("interpn: requires at least 2 arguments",
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
    throw MError("interpn: 4+-D inputs are not yet supported",
                 0, 0, "interpn", "", "m:interpn:rank");
}

// polyfit_reg / polyval_reg → math/elementary/polynomials.cpp
// trapz_reg                 → math/integration/integration.cpp

} // namespace detail

} // namespace numkit::m::builtin
