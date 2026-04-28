// libs/fit/src/MFitInterp.cpp

#include <numkit/m/fit/MFitInterp.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::m::fit {

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

// ── polyfit ───────────────────────────────────────────────────────────
MValue polyfit(Allocator &alloc, const MValue &x, const MValue &y, int deg)
{
    const size_t m = x.numel();
    const int np = deg + 1;

    if (static_cast<size_t>(np) > m)
        throw MError("polyfit: not enough data points",
                     0, 0, "polyfit", "", "m:polyfit:tooFewPoints");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    // Vandermonde matrix A[j, i] = x[i]^(deg - j), stored column-major
    std::vector<double> A(m * np);
    for (size_t i = 0; i < m; ++i)
        for (int j = 0; j < np; ++j)
            A[j * m + i] = std::pow(xd[i], deg - j);

    // Normal equations: ATA * p = AT * y
    std::vector<double> ATA(np * np, 0.0);
    for (int r = 0; r < np; ++r)
        for (int c = 0; c < np; ++c)
            for (size_t i = 0; i < m; ++i)
                ATA[c * np + r] += A[r * m + i] * A[c * m + i];

    std::vector<double> ATy(np, 0.0);
    for (int r = 0; r < np; ++r)
        for (size_t i = 0; i < m; ++i)
            ATy[r] += A[r * m + i] * yd[i];

    // Gaussian elimination with partial pivoting, augmented [ATA | ATy]
    std::vector<double> aug(np * (np + 1));
    for (int r = 0; r < np; ++r) {
        for (int c = 0; c < np; ++c)
            aug[r * (np + 1) + c] = ATA[c * np + r];
        aug[r * (np + 1) + np] = ATy[r];
    }

    for (int k = 0; k < np; ++k) {
        int maxRow = k;
        double maxVal = std::abs(aug[k * (np + 1) + k]);
        for (int r = k + 1; r < np; ++r) {
            const double v = std::abs(aug[r * (np + 1) + k]);
            if (v > maxVal) {
                maxVal = v;
                maxRow = r;
            }
        }
        if (maxRow != k) {
            for (int c = 0; c <= np; ++c)
                std::swap(aug[k * (np + 1) + c], aug[maxRow * (np + 1) + c]);
        }

        const double pivot = aug[k * (np + 1) + k];
        if (std::abs(pivot) < 1e-15)
            throw MError("polyfit: singular matrix",
                         0, 0, "polyfit", "", "m:polyfit:singular");

        for (int c = k; c <= np; ++c)
            aug[k * (np + 1) + c] /= pivot;
        for (int r = 0; r < np; ++r) {
            if (r == k)
                continue;
            const double f = aug[r * (np + 1) + k];
            for (int c = k; c <= np; ++c)
                aug[r * (np + 1) + c] -= f * aug[k * (np + 1) + c];
        }
    }

    auto p = MValue::matrix(1, np, MType::DOUBLE, &alloc);
    for (int j = 0; j < np; ++j)
        p.doubleDataMut()[j] = aug[j * (np + 1) + np];
    return p;
}

// ── polyval ───────────────────────────────────────────────────────────
MValue polyval(Allocator &alloc, const MValue &p, const MValue &x)
{
    const double *pd = p.doubleData();
    const size_t np = p.numel();
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    auto r = createLike(x, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nx; ++i) {
        double val = pd[0];
        for (size_t j = 1; j < np; ++j)
            val = val * xd[i] + pd[j];
        r.doubleDataMut()[i] = val;
    }
    return r;
}

// ── trapz ─────────────────────────────────────────────────────────────
MValue trapz(Allocator &alloc, const MValue &y)
{
    const double *yd = y.doubleData();
    const size_t n = y.numel();
    double s = 0.0;
    for (size_t i = 1; i < n; ++i)
        s += 0.5 * (yd[i - 1] + yd[i]);
    return MValue::scalar(s, &alloc);
}

MValue trapz(Allocator &alloc, const MValue &x, const MValue &y)
{
    const size_t n = x.numel();
    if (y.numel() != n)
        throw MError("trapz: x and y must have same length",
                     0, 0, "trapz", "", "m:trapz:lengthMismatch");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    double s = 0.0;
    for (size_t i = 1; i < n; ++i)
        s += 0.5 * (yd[i - 1] + yd[i]) * (xd[i] - xd[i - 1]);
    return MValue::scalar(s, &alloc);
}

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

void pchip_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("pchip: requires 3 arguments",
                     0, 0, "pchip", "", "m:pchip:nargin");
    outs[0] = pchip(ctx.engine->allocator(), args[0], args[1], args[2]);
}

void polyfit_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("polyfit: requires 3 arguments",
                     0, 0, "polyfit", "", "m:polyfit:nargin");
    outs[0] = polyfit(ctx.engine->allocator(),
                      args[0], args[1],
                      static_cast<int>(args[2].toScalar()));
}

void polyval_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("polyval: requires 2 arguments",
                     0, 0, "polyval", "", "m:polyval:nargin");
    outs[0] = polyval(ctx.engine->allocator(), args[0], args[1]);
}

void trapz_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("trapz: requires at least 1 argument",
                     0, 0, "trapz", "", "m:trapz:nargin");
    if (args.size() == 1)
        outs[0] = trapz(ctx.engine->allocator(), args[0]);
    else
        outs[0] = trapz(ctx.engine->allocator(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::m::fit
