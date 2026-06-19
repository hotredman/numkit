// toolboxes/builtin/src/math/integration/integration.cpp
//
// Numerical-calculus builtins:
//   - gradient / gradient2 — central differences (vector + 2D matrix)
//   - cumtrapz             — cumulative trapezoidal integration (1-D)
//   - integral             — adaptive Gauss-Kronrod definite integral
// fzero lives in math/optim/fzero.cpp (uses the same callback helper).

#include <numkit/math/integration/integration.hpp>
#include <numkit/math/integration/integration_detail.hpp>

#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include "../_callback_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace numkit::math {

using namespace detail;   // integration impl-helpers (gradientND/cumtrapz*/trapzImpl/...)

namespace cb = ::numkit::math::detail::callback;

namespace detail {

// One-pass forward/central/backward difference along a contiguous slice
// of length n with stride 1. dst[i] = (src[i+1] - src[i-1]) / (2h) for
// interior; dst[0] = (src[1] - src[0]) / h; dst[n-1] = (src[n-1] -
// src[n-2]) / h.
void gradient1D(const double *src, double *dst, size_t n, double h)
{
    if (n == 0) return;
    if (n == 1) { dst[0] = 0.0; return; }
    const double inv2h = 0.5 / h;
    const double invH  = 1.0 / h;
    dst[0]     = (src[1] - src[0]) * invH;
    dst[n - 1] = (src[n - 1] - src[n - 2]) * invH;
    for (size_t i = 1; i + 1 < n; ++i)
        dst[i] = (src[i + 1] - src[i - 1]) * inv2h;
}

// 2D gradient along dim-2 (columns) for a matrix in column-major layout.
void gradientAlongCols(const double *src, double *dst, size_t R, size_t C, double h)
{
    if (C == 0) return;
    const double inv2h = 0.5 / h;
    const double invH  = 1.0 / h;
    if (C == 1) {
        for (size_t r = 0; r < R; ++r) dst[r] = 0.0;
        return;
    }
    for (size_t r = 0; r < R; ++r) {
        dst[r]               = (src[r + R]              - src[r])           * invH;
        dst[r + (C - 1) * R] = (src[r + (C - 1) * R]    - src[r + (C - 2) * R]) * invH;
        for (size_t c = 1; c + 1 < C; ++c)
            dst[r + c * R] = (src[r + (c + 1) * R] - src[r + (c - 1) * R]) * inv2h;
    }
}

// 2D gradient along dim-1 (rows) for a matrix in column-major layout.
void gradientAlongRows(const double *src, double *dst, size_t R, size_t C, double h)
{
    if (R == 0) return;
    const double invH = 1.0 / h;
    if (R == 1) {
        for (size_t c = 0; c < C; ++c) dst[c * R] = 0.0;
        return;
    }
    for (size_t c = 0; c < C; ++c)
        gradient1D(src + c * R, dst + c * R, R, h);
    (void)invH;
}

// Gradient along an arbitrary dimension `dim` (0-based) of an N-D array in
// column-major layout. Central differences on the interior, one-sided at the
// ends, uniform scalar spacing h. A singleton dimension yields all zeros.
// Generalises gradientAlongCols (dim==1) / gradientAlongRows (dim==0).
void gradientAlongDim(const double *src, double *dst, const Dims &shape,
                      int dim, double h)
{
    const size_t N = shape.numel();
    if (N == 0) return;
    const size_t L = shape.dim(dim);
    if (L <= 1) { for (size_t i = 0; i < N; ++i) dst[i] = 0.0; return; }
    size_t inner = 1;
    for (int k = 0; k < dim; ++k) inner *= shape.dim(k);
    const size_t block = L * inner;
    const size_t outer = N / block;
    const double inv2h = 0.5 / h, invH = 1.0 / h;
    for (size_t o = 0; o < outer; ++o) {
        for (size_t in = 0; in < inner; ++in) {
            const size_t base = o * block + in;
            const size_t last = base + (L - 1) * inner;
            dst[base] = (src[base + inner] - src[base]) * invH;
            dst[last] = (src[last] - src[last - inner]) * invH;
            for (size_t k = 1; k + 1 < L; ++k)
                dst[base + k * inner] =
                    (src[base + (k + 1) * inner] - src[base + (k - 1) * inner]) * inv2h;
        }
    }
}

// One dimension's discrete-Laplacian term (g) along a strided line of
// length n (element stride s, uniform spacing h). Centered second
// difference on the interior, divided by 2h^2; the boundary values are a
// linear extrapolation of the interior g (n>3), or copied from the single
// interior point (n==3), or zero (n<3). Matches MATLAB del2's per-pass g.
void del2Line(const double *src, double *dst, size_t n, size_t s, double h)
{
    for (size_t i = 0; i < n; ++i) dst[i * s] = 0.0;
    if (n < 3) return;
    const double inv = 1.0 / (2.0 * h * h);
    for (size_t i = 1; i + 1 < n; ++i)
        dst[i * s] = (src[(i + 1) * s] - 2.0 * src[i * s] + src[(i - 1) * s]) * inv;
    if (n == 3) {
        dst[0]           = dst[1 * s];
        dst[2 * s]       = dst[1 * s];
    } else {
        dst[0]           = 2.0 * dst[1 * s]       - dst[2 * s];
        dst[(n - 1) * s] = 2.0 * dst[(n - 2) * s] - dst[(n - 3) * s];
    }
}

Value toDoubleCopy(const Value &x, std::pmr::memory_resource *mr)
{
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (x.type() == ValueType::DOUBLE) {
        std::memcpy(r.doubleDataMut(), x.doubleData(),
                    x.numel() * sizeof(double));
    } else {
        double *dst = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = x.elemAsDouble(i);
    }
    return r;
}

} // namespace

namespace detail {
// Real / imaginary parts of a COMPLEX value as same-shape DOUBLE arrays, and
// the inverse combine. (toDoubleCopy can't be used on complex — elemAsDouble
// rejects a nonzero imaginary part.)
Value realPartCopy(const Value &f, std::pmr::memory_resource *mr)
{
    auto r = createLike(f, ValueType::DOUBLE, mr);
    double *d = r.doubleDataMut();
    const Complex *c = f.complexData();
    const size_t n = f.numel();
    for (size_t i = 0; i < n; ++i) d[i] = c[i].real();
    return r;
}
Value imagPartCopy(const Value &f, std::pmr::memory_resource *mr)
{
    auto r = createLike(f, ValueType::DOUBLE, mr);
    double *d = r.doubleDataMut();
    const Complex *c = f.complexData();
    const size_t n = f.numel();
    for (size_t i = 0; i < n; ++i) d[i] = c[i].imag();
    return r;
}
Value combineComplexParts(const Value &re, const Value &im, std::pmr::memory_resource *mr)
{
    auto out = createLike(re, ValueType::COMPLEX, mr);
    Complex *o = out.complexDataMut();
    const double *r = re.doubleData(), *m = im.doubleData();
    const size_t n = out.numel();
    for (size_t i = 0; i < n; ++i) o[i] = Complex(r[i], m[i]);
    return out;
}
} // namespace

Value gradient(const Value &f, double h, std::pmr::memory_resource *mr)
{
    if (h <= 0)
        throw Error("gradient: spacing h must be positive",
                     0, 0, "gradient", "", "numkit:gradient:badSpacing");
    // Complex: gradient real + imaginary parts separately, recombine (MATLAB).
    if (f.type() == ValueType::COMPLEX) {
        Value gr = gradient(realPartCopy(f, mr), h, mr);
        Value gi = gradient(imagPartCopy(f, mr), h, mr);
        return combineComplexParts(gr, gi, mr);
    }

    auto src = toDoubleCopy(f, mr);
    auto out = createLike(f, ValueType::DOUBLE, mr);
    const auto &d = f.dims();

    if (f.dims().isVector() || f.isScalar()) {
        gradient1D(src.doubleData(), out.doubleDataMut(), f.numel(), h);
        return out;
    }
    if (d.is3D() || d.ndim() > 2) {
        // N-D single output = gradient along the dim-2 (x / column) direction,
        // i.e. 0-based dim 1, matching MATLAB. (bugs/builtin/gradient-3d.md)
        gradientAlongDim(src.doubleData(), out.doubleDataMut(), d, 1, h);
        return out;
    }
    gradientAlongCols(src.doubleData(), out.doubleDataMut(),
                      d.rows(), d.cols(), h);
    return out;
}

Value del2(const Value &u, double h, std::pmr::memory_resource *mr)
{
    if (h <= 0)
        throw Error("del2: spacing h must be positive",
                     0, 0, "del2", "", "numkit:del2:badSpacing");
    if (u.type() == ValueType::COMPLEX)
        throw Error("del2: complex inputs are not supported",
                     0, 0, "del2", "", "numkit:del2:complex");
    const auto &d = u.dims();
    if (d.is3D() || d.ndim() > 2)
        throw Error("del2: only 1D vector and 2D matrix inputs are supported",
                     0, 0, "del2", "", "numkit:del2:rank");

    auto src = toDoubleCopy(u, mr);
    auto out = createLike(u, ValueType::DOUBLE, mr);
    const double *sd = src.doubleData();
    double *od = out.doubleDataMut();
    const size_t N = u.numel();
    for (size_t i = 0; i < N; ++i) od[i] = 0.0;
    if (N == 0) return out;

    // MATLAB divides the summed per-dimension Laplacian by ndims(U); a
    // vector still has ndims == 2, so the divisor is 2 for both shapes.
    if (u.dims().isVector() || u.isScalar()) {
        ScratchArena arena(mr);
        auto g = ScratchVec<double>(&arena); g.assign(N, 0.0);
        del2Line(sd, g.data(), N, 1, h);
        for (size_t i = 0; i < N; ++i) od[i] = g[i] * 0.5;
        return out;
    }
    const size_t R = d.rows(), C = d.cols();
    ScratchArena arena(mr);
    auto g1 = ScratchVec<double>(&arena); g1.assign(N, 0.0);
    auto g2 = ScratchVec<double>(&arena); g2.assign(N, 0.0);
    // dim-1: down each column (stride 1, length R).
    for (size_t c = 0; c < C; ++c) del2Line(sd + c * R, g1.data() + c * R, R, 1, h);
    // dim-2: across each row (stride R, length C).
    for (size_t r = 0; r < R; ++r) del2Line(sd + r, g2.data() + r, C, R, h);
    for (size_t i = 0; i < N; ++i) od[i] = (g1[i] + g2[i]) * 0.5;
    return out;
}

std::tuple<Value, Value>
gradient2(const Value &f, double hx, double hy, std::pmr::memory_resource *mr)
{
    if (hx <= 0 || hy <= 0)
        throw Error("gradient: spacing arguments must be positive",
                     0, 0, "gradient", "", "numkit:gradient:badSpacing");
    // Complex: gradient real + imaginary parts separately, recombine (MATLAB).
    if (f.type() == ValueType::COMPLEX) {
        auto [grx, gry] = gradient2(realPartCopy(f, mr), hx, hy, mr);
        auto [gix, giy] = gradient2(imagPartCopy(f, mr), hx, hy, mr);
        return std::make_tuple(combineComplexParts(grx, gix, mr),
                               combineComplexParts(gry, giy, mr));
    }
    const auto &d = f.dims();
    if (d.is3D() || d.ndim() > 2)
        throw Error("gradient: 2-output form requires a 2D matrix input",
                     0, 0, "gradient", "", "numkit:gradient:rank");

    auto src = toDoubleCopy(f, mr);
    auto fx = createLike(f, ValueType::DOUBLE, mr);
    auto fy = createLike(f, ValueType::DOUBLE, mr);

    if (f.dims().isVector() || f.isScalar()) {
        gradient1D(src.doubleData(), fx.doubleDataMut(), f.numel(), hx);
        gradient1D(src.doubleData(), fy.doubleDataMut(), f.numel(), hy);
        return std::make_tuple(std::move(fx), std::move(fy));
    }
    gradientAlongCols(src.doubleData(), fx.doubleDataMut(),
                      d.rows(), d.cols(), hx);
    gradientAlongRows(src.doubleData(), fy.doubleDataMut(),
                      d.rows(), d.cols(), hy);
    return std::make_tuple(std::move(fx), std::move(fy));
}

namespace detail {
// N-D gradient: emit `nout` arrays. Output o is taken along dimension perm(o)
// where perm = {1, 0, 2, 3, ...} (0-based) — i.e. out0 = dim-2 (x), out1 =
// dim-1 (y), out_k = dim-(k+1) for k>=2, matching MATLAB. `hs[o]` is the
// scalar spacing for output o. Complex F is gradiented part-wise then
// recombined. Throws if more outputs than dimensions are requested.
std::pmr::vector<Value> gradientND(const Value &f, const double *hs, size_t nh,
                                   size_t nout, std::pmr::memory_resource *mr)
{
    if (f.type() == ValueType::COMPLEX) {
        auto re = gradientND(realPartCopy(f, mr), hs, nh, nout, mr);
        auto im = gradientND(imagPartCopy(f, mr), hs, nh, nout, mr);
        std::pmr::vector<Value> out(mr);
        out.reserve(nout);
        for (size_t o = 0; o < nout; ++o)
            out.push_back(combineComplexParts(re[o], im[o], mr));
        return out;
    }

    const Dims &shape = f.dims();
    const int R = shape.ndim();
    auto src = toDoubleCopy(f, mr);
    std::pmr::vector<Value> outs(mr);
    outs.reserve(nout);
    for (size_t o = 0; o < nout; ++o) {
        const int dim = (o == 0) ? 1 : (o == 1) ? 0 : static_cast<int>(o);
        if (dim >= R)
            throw Error("gradient: too many output arguments for the input "
                        "dimensionality",
                        0, 0, "gradient", "", "numkit:gradient:nargout");
        const double h = (o < nh) ? hs[o] : 1.0;
        if (h <= 0)
            throw Error("gradient: spacing arguments must be positive",
                        0, 0, "gradient", "", "numkit:gradient:badSpacing");
        Value out = createLike(f, ValueType::DOUBLE, mr);
        gradientAlongDim(src.doubleData(), out.doubleDataMut(), shape, dim, h);
        outs.push_back(std::move(out));
    }
    return outs;
}
} // namespace

// ── cumtrapz ─────────────────────────────────────────────────────────
namespace detail {

Value cumtrapzVector(const double *y, const double *x, size_t n, const Dims &shape, bool unitSpacing, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(shape.rows(), shape.cols(), ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    if (n == 0) return out;
    dst[0] = 0.0;
    for (size_t i = 1; i < n; ++i) {
        const double dx = unitSpacing ? 1.0 : (x[i] - x[i - 1]);
        dst[i] = dst[i - 1] + 0.5 * (y[i - 1] + y[i]) * dx;
    }
    return out;
}

} // namespace

namespace detail {

// Matrix form: integrate along columns (MATLAB default for matrix
// inputs — first non-singleton dim is rows). xData==nullptr → unit
// spacing. xData layout: same shape as src, used per-column.
Value cumtrapzMatrixCols(const double *src, const double *xData, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    if (rows == 0 || cols == 0) return out;
    for (size_t c = 0; c < cols; ++c) {
        const double *col = src + c * rows;
        const double *xCol = xData ? xData + c * rows : nullptr;
        double *dCol = dst + c * rows;
        dCol[0] = 0.0;
        for (size_t r = 1; r < rows; ++r) {
            const double dx = xCol ? (xCol[r] - xCol[r - 1]) : 1.0;
            dCol[r] = dCol[r - 1] + 0.5 * (col[r - 1] + col[r]) * dx;
        }
    }
    return out;
}

// Row-wise (dim 2) cumulative trapezoid: each row integrated across cols.
Value cumtrapzMatrixRows(const double *src, const double *xData, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    if (rows == 0 || cols == 0) return out;
    for (size_t r = 0; r < rows; ++r) {
        dst[r] = 0.0;                       // first column = 0
        for (size_t c = 1; c < cols; ++c) {
            const double dx = xData ? (xData[c * rows + r] - xData[(c - 1) * rows + r]) : 1.0;
            dst[c * rows + r] = dst[(c - 1) * rows + r]
                              + 0.5 * (src[(c - 1) * rows + r] + src[c * rows + r]) * dx;
        }
    }
    return out;
}

// ── COMPLEX counterparts (the integration variable x stays real) ────────
Value cumtrapzVectorC(const Complex *y, const double *x, size_t n, const Dims &shape,
                      bool unitSpacing, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(shape.rows(), shape.cols(), ValueType::COMPLEX, mr);
    Complex *dst = out.complexDataMut();
    if (n == 0) return out;
    dst[0] = Complex(0.0, 0.0);
    for (size_t i = 1; i < n; ++i) {
        const double dx = unitSpacing ? 1.0 : (x[i] - x[i - 1]);
        dst[i] = dst[i - 1] + 0.5 * (y[i - 1] + y[i]) * dx;
    }
    return out;
}

Value cumtrapzMatrixColsC(const Complex *src, const double *xData, size_t rows, size_t cols,
                          std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(rows, cols, ValueType::COMPLEX, mr);
    Complex *dst = out.complexDataMut();
    if (rows == 0 || cols == 0) return out;
    for (size_t c = 0; c < cols; ++c) {
        const Complex *col = src + c * rows;
        const double *xCol = xData ? xData + c * rows : nullptr;
        Complex *dCol = dst + c * rows;
        dCol[0] = Complex(0.0, 0.0);
        for (size_t r = 1; r < rows; ++r) {
            const double dx = xCol ? (xCol[r] - xCol[r - 1]) : 1.0;
            dCol[r] = dCol[r - 1] + 0.5 * (col[r - 1] + col[r]) * dx;
        }
    }
    return out;
}

Value cumtrapzMatrixRowsC(const Complex *src, const double *xData, size_t rows, size_t cols,
                          std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(rows, cols, ValueType::COMPLEX, mr);
    Complex *dst = out.complexDataMut();
    if (rows == 0 || cols == 0) return out;
    for (size_t r = 0; r < rows; ++r) {
        dst[r] = Complex(0.0, 0.0);
        for (size_t c = 1; c < cols; ++c) {
            const double dx = xData ? (xData[c * rows + r] - xData[(c - 1) * rows + r]) : 1.0;
            dst[c * rows + r] = dst[(c - 1) * rows + r]
                              + 0.5 * (src[(c - 1) * rows + r] + src[c * rows + r]) * dx;
        }
    }
    return out;
}

} // namespace

// cumtrapz(Y, dim): unit-spacing cumulative trapezoid along dim (1 or 2).
// A vector is treated as the matrix it is (1×N or N×1): integrating along
// the singleton dimension is a no-op (all zeros), matching MATLAB — e.g.
// cumtrapz([1 2 3 4], 1) → [0 0 0 0], cumtrapz([1 2 3 4], 2) → cumulative.
Value cumtrapzDim(const Value &y, int dim, std::pmr::memory_resource *mr)
{
    const size_t rows = y.dims().rows(), cols = y.dims().cols();
    if (dim <= 0) dim = 1;
    if (y.type() == ValueType::COMPLEX) {
        const Complex *yc = y.complexData();
        if (dim == 2) return cumtrapzMatrixRowsC(yc, nullptr, rows, cols, mr);
        return cumtrapzMatrixColsC(yc, nullptr, rows, cols, mr);
    }
    auto ys = toDoubleCopy(y, mr);
    if (dim == 2)
        return cumtrapzMatrixRows(ys.doubleData(), nullptr, rows, cols, mr);
    return cumtrapzMatrixCols(ys.doubleData(), nullptr, rows, cols, mr);
}

Value cumtrapz(const Value &y, std::pmr::memory_resource *mr)
{
    if (y.type() == ValueType::COMPLEX) {
        const Complex *yc = y.complexData();
        if (y.dims().isVector() || y.isScalar())
            return cumtrapzVectorC(yc, nullptr, y.numel(), y.dims(), /*unitSpacing=*/true, mr);
        return cumtrapzMatrixColsC(yc, nullptr, y.dims().rows(), y.dims().cols(), mr);
    }
    auto ys = toDoubleCopy(y, mr);
    if (y.dims().isVector() || y.isScalar()) {
        return cumtrapzVector(ys.doubleData(), nullptr, y.numel(), y.dims(), /*unitSpacing=*/true, mr);
    }
    return cumtrapzMatrixCols(ys.doubleData(), nullptr, y.dims().rows(), y.dims().cols(), mr);
}

Value cumtrapz(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    // The X coordinate must be real; Y may be complex.
    if (x.type() == ValueType::COMPLEX)
        throw Error("cumtrapz: the X coordinate must be real",
                     0, 0, "cumtrapz", "", "numkit:cumtrapz:complexX");
    const bool yIsC = (y.type() == ValueType::COMPLEX);
    Value ys;
    if (!yIsC) ys = toDoubleCopy(y, mr);
    const double *yd = yIsC ? nullptr : ys.doubleData();
    const Complex *yc = yIsC ? y.complexData() : nullptr;

    if (y.dims().isVector() || y.isScalar()) {
        if (!x.dims().isVector() && !x.isScalar())
            throw Error("cumtrapz: when y is a vector, x must also be a vector",
                         0, 0, "cumtrapz", "", "numkit:cumtrapz:shapeMismatch");
        if (x.numel() != y.numel())
            throw Error("cumtrapz: x and y must have the same length",
                         0, 0, "cumtrapz", "", "numkit:cumtrapz:lengthMismatch");
        auto xs = toDoubleCopy(x, mr);
        return yIsC ? cumtrapzVectorC(yc, xs.doubleData(), y.numel(), y.dims(), /*unitSpacing=*/false, mr)
                    : cumtrapzVector(yd, xs.doubleData(), y.numel(), y.dims(), /*unitSpacing=*/false, mr);
    }

    // Matrix y. x may be a vector (broadcast across every column) or
    // matrix of the same shape as y (per-column spacing).
    const size_t rows = y.dims().rows();
    const size_t cols = y.dims().cols();
    auto xs = toDoubleCopy(x, mr);
    if (x.dims().isVector() && x.numel() == rows) {
        // Broadcast x to every column.
        auto xMat = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
        double *dx = xMat.doubleDataMut();
        const double *src = xs.doubleData();
        for (size_t c = 0; c < cols; ++c)
            std::memcpy(dx + c * rows, src, rows * sizeof(double));
        return yIsC ? cumtrapzMatrixColsC(yc, dx, rows, cols, mr)
                    : cumtrapzMatrixCols(yd, dx, rows, cols, mr);
    }
    if (x.dims().rows() != rows || x.dims().cols() != cols)
        throw Error("cumtrapz: x size must match y or be a column-length vector",
                     0, 0, "cumtrapz", "", "numkit:cumtrapz:shapeMismatch");
    return yIsC ? cumtrapzMatrixColsC(yc, xs.doubleData(), rows, cols, mr)
                : cumtrapzMatrixCols(yd, xs.doubleData(), rows, cols, mr);
}

// ── integral (adaptive Gauss-Kronrod) ────────────────────────────────
namespace detail {


std::pair<double, double>
gaussKronrod15(FnHandle fn, double a, double b,
               std::pmr::memory_resource *mr)
{
    const double half  = 0.5 * (b - a);
    const double mid   = 0.5 * (b + a);
    double K = 0.0, G = 0.0;
    for (int i = 0; i < 15; ++i) {
        const double x  = mid + half * kKronrodX[i];
        const double fv = cb::evalScalar(fn, x, mr);
        K += kKronrodW[i] * fv;
        if (i % 2 == 1)
            G += kGaussW[i / 2] * fv;
    }
    return {half * K, half * G};
}

double adaptiveIntegral(FnHandle fn, double a, double b,
                        double absTol, int depth, int maxDepth,
                        std::pmr::memory_resource *mr)
{
    auto [K, G] = gaussKronrod15(fn, a, b, mr);
    const double err = std::abs(K - G);
    if (err < absTol || depth >= maxDepth) return K;
    const double mid = 0.5 * (a + b);
    return adaptiveIntegral(fn, a, mid, absTol * 0.5, depth + 1, maxDepth, mr)
         + adaptiveIntegral(fn, mid, b, absTol * 0.5, depth + 1, maxDepth, mr);
}

} // namespace

Value integral(FnHandle fn, double a, double b, double absTol,
               std::pmr::memory_resource *mr)
{
    if (!std::isfinite(a) || !std::isfinite(b))
        throw Error("integral: bounds must be finite",
                     0, 0, "integral", "", "numkit:integral:badBounds");
    if (absTol <= 0)
        throw Error("integral: absTol must be positive",
                     0, 0, "integral", "", "numkit:integral:badTol");
    const double sign = (b < a) ? -1.0 : 1.0;
    if (b < a) std::swap(a, b);
    if (a == b) return Value::scalar(0.0, mr);
    constexpr int kMaxDepth = 20;
    const double r = adaptiveIntegral(fn, a, b, absTol, 0, kMaxDepth, mr);
    return Value::scalar(sign * r, mr);
}

Value integral2(FnHandle fn, double a, double b, double c, double d,
                double absTol, std::pmr::memory_resource *mr)
{
    // Iterated quadrature: outer sweep over x; at each x, an inner sweep over
    // y of fn(x, ·). Both reuse the adaptive 1-D `integral`. (function_ref is
    // non-owning, but every nested call is synchronous, so the lambdas and the
    // captured x outlive the calls that reference them.)
    double xval = 0.0;
    auto innerFn = [&fn, &xval](Span<const Value> ay, Span<Value> oy,
                                std::pmr::memory_resource *m) {
        Value xy[2] = { Value::scalar(xval, m), ay[0] };
        fn(Span<const Value>(xy, 2), oy, m);
    };
    auto outerFn = [&](Span<const Value> ax, Span<Value> ox,
                       std::pmr::memory_resource *m) {
        xval = ax[0].toScalar();
        ox[0] = integral(innerFn, c, d, absTol, m);
    };
    return integral(outerFn, a, b, absTol, mr);
}

Value integral3(FnHandle fn, double a, double b, double c, double d,
                double e, double f, double absTol,
                std::pmr::memory_resource *mr)
{
    double xval = 0.0, yval = 0.0;
    auto innermostFn = [&fn, &xval, &yval](Span<const Value> az, Span<Value> oz,
                                           std::pmr::memory_resource *m) {
        Value xyz[3] = { Value::scalar(xval, m), Value::scalar(yval, m), az[0] };
        fn(Span<const Value>(xyz, 3), oz, m);
    };
    auto middleFn = [&](Span<const Value> ay, Span<Value> oy,
                        std::pmr::memory_resource *m) {
        yval = ay[0].toScalar();
        oy[0] = integral(innermostFn, e, f, absTol, m);
    };
    auto outerFn = [&](Span<const Value> ax, Span<Value> ox,
                       std::pmr::memory_resource *m) {
        xval = ax[0].toScalar();
        ox[0] = integral(middleFn, c, d, absTol, m);
    };
    return integral(outerFn, a, b, absTol, mr);
}

namespace detail {

// Trapezoidal integral of `y` along 1-based `dim`. `xData` (length = size of
// y along dim) gives non-uniform spacing; nullptr = unit spacing. A vector
// reduces to a scalar; a matrix reduces along `dim` (MATLAB trapz semantics:
// trapz(M) = 1xC over columns, trapz(M,2) = Rx1 over rows).
Value trapzImpl(const Value &y, int dim, const double *xData,
                std::pmr::memory_resource *mr)
{
    const size_t R = y.dims().rows(), C = y.dims().cols();
    // Complex y: trapezoidal sum over Complex storage (the integration
    // variable x — xData — is always real). MATLAB accepts complex y.
    if (y.type() == ValueType::COMPLEX) {
        const Complex *yc = y.complexData();
        if (dim != 2) {
            Value out = Value::matrix(1, C, ValueType::COMPLEX, mr);
            Complex *o = out.complexDataMut();
            for (size_t c = 0; c < C; ++c) {
                Complex s(0.0, 0.0);
                for (size_t r = 1; r < R; ++r)
                    s += 0.5 * (yc[c * R + r - 1] + yc[c * R + r])
                             * (xData ? (xData[r] - xData[r - 1]) : 1.0);
                o[c] = s;
            }
            return out;
        }
        Value out = Value::matrix(R, 1, ValueType::COMPLEX, mr);
        Complex *o = out.complexDataMut();
        for (size_t r = 0; r < R; ++r) {
            Complex s(0.0, 0.0);
            for (size_t c = 1; c < C; ++c)
                s += 0.5 * (yc[(c - 1) * R + r] + yc[c * R + r])
                         * (xData ? (xData[c] - xData[c - 1]) : 1.0);
            o[r] = s;
        }
        return out;
    }
    const double *yd = y.doubleData();
    if (dim != 2) {  // dim 1 (default for column reduction)
        Value out = Value::matrix(1, C, ValueType::DOUBLE, mr);
        double *o = out.doubleDataMut();
        for (size_t c = 0; c < C; ++c) {
            double s = 0.0;
            for (size_t r = 1; r < R; ++r)
                s += 0.5 * (yd[c * R + r - 1] + yd[c * R + r])
                         * (xData ? (xData[r] - xData[r - 1]) : 1.0);
            o[c] = s;
        }
        return out;
    }
    Value out = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t r = 0; r < R; ++r) {
        double s = 0.0;
        for (size_t c = 1; c < C; ++c)
            s += 0.5 * (yd[(c - 1) * R + r] + yd[c * R + r])
                     * (xData ? (xData[c] - xData[c - 1]) : 1.0);
        o[r] = s;
    }
    return out;
}

int trapzFirstNonSingletonDim(const Value &y)
{
    const auto &d = y.dims();
    for (int k = 0; k < d.ndim(); ++k)
        if (d.dim(k) > 1) return k + 1;
    return 1;
}

} // namespace

// ── trapz (uniform / explicit spacing) — public compute ────────
Value trapz(const Value &y, std::pmr::memory_resource *mr)
{
    if (y.numel() == 0) return Value::scalar(0.0, mr);
    // Integrate along the first non-singleton dimension (MATLAB): a vector
    // reduces to a scalar, a matrix reduces over its columns.
    return trapzImpl(y, trapzFirstNonSingletonDim(y), nullptr, mr);
}

Value trapz(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const int dim = trapzFirstNonSingletonDim(y);
    const size_t along = (dim != 2) ? y.dims().rows() : y.dims().cols();
    if (x.numel() != along)
        throw Error("trapz: x and y must have same length",
                     0, 0, "trapz", "", "numkit:trapz:lengthMismatch");
    return trapzImpl(y, dim, x.doubleData(), mr);
}

} // namespace numkit::math
