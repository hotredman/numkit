// libs/builtin/src/math/integration/integration.cpp
//
// Numerical-calculus builtins:
//   - gradient / gradient2 — central differences (vector + 2D matrix)
//   - cumtrapz             — cumulative trapezoidal integration (1-D)
//   - integral             — adaptive Gauss-Kronrod definite integral
// fzero lives in math/optim/fzero.cpp (uses the same callback helper).

#include <numkit/builtin/math/integration/integration.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "../_callback_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace numkit::builtin {

namespace cb = ::numkit::builtin::detail::callback;

namespace {

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

Value gradient(const Value &f, double h, std::pmr::memory_resource *mr)
{
    if (h <= 0)
        throw Error("gradient: spacing h must be positive",
                     0, 0, "gradient", "", "m:gradient:badSpacing");
    if (f.type() == ValueType::COMPLEX)
        throw Error("gradient: complex inputs are not supported",
                     0, 0, "gradient", "", "m:gradient:complex");

    auto src = toDoubleCopy(f, mr);
    auto out = createLike(f, ValueType::DOUBLE, mr);
    const auto &d = f.dims();

    if (f.dims().isVector() || f.isScalar()) {
        gradient1D(src.doubleData(), out.doubleDataMut(), f.numel(), h);
        return out;
    }
    if (d.is3D() || d.ndim() > 2)
        throw Error("gradient: only 1D vector and 2D matrix inputs are supported",
                     0, 0, "gradient", "", "m:gradient:rank");
    gradientAlongCols(src.doubleData(), out.doubleDataMut(),
                      d.rows(), d.cols(), h);
    return out;
}

std::tuple<Value, Value>
gradient2(const Value &f, double hx, double hy, std::pmr::memory_resource *mr)
{
    if (hx <= 0 || hy <= 0)
        throw Error("gradient: spacing arguments must be positive",
                     0, 0, "gradient", "", "m:gradient:badSpacing");
    if (f.type() == ValueType::COMPLEX)
        throw Error("gradient: complex inputs are not supported",
                     0, 0, "gradient", "", "m:gradient:complex");
    const auto &d = f.dims();
    if (d.is3D() || d.ndim() > 2)
        throw Error("gradient: 2-output form requires a 2D matrix input",
                     0, 0, "gradient", "", "m:gradient:rank");

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

// ── cumtrapz ─────────────────────────────────────────────────────────
namespace {

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

namespace {

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

} // namespace

Value cumtrapz(const Value &y, std::pmr::memory_resource *mr)
{
    if (y.type() == ValueType::COMPLEX)
        throw Error("cumtrapz: complex inputs are not supported",
                     0, 0, "cumtrapz", "", "m:cumtrapz:complex");

    auto ys = toDoubleCopy(y, mr);
    if (y.dims().isVector() || y.isScalar()) {
        return cumtrapzVector(ys.doubleData(), nullptr, y.numel(), y.dims(), /*unitSpacing=*/true, mr);
    }
    return cumtrapzMatrixCols(ys.doubleData(), nullptr, y.dims().rows(), y.dims().cols(), mr);
}

Value cumtrapz(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX || y.type() == ValueType::COMPLEX)
        throw Error("cumtrapz: complex inputs are not supported",
                     0, 0, "cumtrapz", "", "m:cumtrapz:complex");

    auto ys = toDoubleCopy(y, mr);
    if (y.dims().isVector() || y.isScalar()) {
        if (!x.dims().isVector() && !x.isScalar())
            throw Error("cumtrapz: when y is a vector, x must also be a vector",
                         0, 0, "cumtrapz", "", "m:cumtrapz:shapeMismatch");
        if (x.numel() != y.numel())
            throw Error("cumtrapz: x and y must have the same length",
                         0, 0, "cumtrapz", "", "m:cumtrapz:lengthMismatch");
        auto xs = toDoubleCopy(x, mr);
        return cumtrapzVector(ys.doubleData(), xs.doubleData(), y.numel(), y.dims(), /*unitSpacing=*/false, mr);
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
        return cumtrapzMatrixCols(ys.doubleData(), dx, rows, cols, mr);
    }
    if (x.dims().rows() != rows || x.dims().cols() != cols)
        throw Error("cumtrapz: x size must match y or be a column-length vector",
                     0, 0, "cumtrapz", "", "m:cumtrapz:shapeMismatch");
    return cumtrapzMatrixCols(ys.doubleData(), xs.doubleData(), rows, cols, mr);
}

// ── integral (adaptive Gauss-Kronrod) ────────────────────────────────
namespace {

// 15-point Gauss-Kronrod nodes (symmetric about 0) and weights.
// Source: Davis & Rabinowitz, "Methods of Numerical Integration".
constexpr double kKronrodX[15] = {
    -0.991455371120813,
    -0.949107912342759,
    -0.864864423359769,
    -0.741531185599394,
    -0.586087235467691,
    -0.405845151377397,
    -0.207784955007898,
     0.0,
     0.207784955007898,
     0.405845151377397,
     0.586087235467691,
     0.741531185599394,
     0.864864423359769,
     0.949107912342759,
     0.991455371120813,
};
constexpr double kKronrodW[15] = {
    0.022935322010529,
    0.063092092629979,
    0.104790010322250,
    0.140653259715525,
    0.169004726639267,
    0.190350578064785,
    0.204432940075298,
    0.209482141084728,
    0.204432940075298,
    0.190350578064785,
    0.169004726639267,
    0.140653259715525,
    0.104790010322250,
    0.063092092629979,
    0.022935322010529,
};
constexpr double kGaussW[7] = {
    0.129484966168870,
    0.279705391489277,
    0.381830050505119,
    0.417959183673469,
    0.381830050505119,
    0.279705391489277,
    0.129484966168870,
};

std::pair<double, double>
gaussKronrod15(FnHandle fn, double a, double b)
{
    const double half  = 0.5 * (b - a);
    const double mid   = 0.5 * (b + a);
    double K = 0.0, G = 0.0;
    for (int i = 0; i < 15; ++i) {
        const double x  = mid + half * kKronrodX[i];
        const double fv = cb::evalScalar(fn, x);
        K += kKronrodW[i] * fv;
        if (i % 2 == 1)
            G += kGaussW[i / 2] * fv;
    }
    return {half * K, half * G};
}

double adaptiveIntegral(FnHandle fn, double a, double b,
                        double absTol, int depth, int maxDepth)
{
    auto [K, G] = gaussKronrod15(fn, a, b);
    const double err = std::abs(K - G);
    if (err < absTol || depth >= maxDepth) return K;
    const double mid = 0.5 * (a + b);
    return adaptiveIntegral(fn, a, mid, absTol * 0.5, depth + 1, maxDepth)
         + adaptiveIntegral(fn, mid, b, absTol * 0.5, depth + 1, maxDepth);
}

} // namespace

Value integral(FnHandle fn, double a, double b, double absTol,
               std::pmr::memory_resource *mr)
{
    if (!std::isfinite(a) || !std::isfinite(b))
        throw Error("integral: bounds must be finite",
                     0, 0, "integral", "", "m:integral:badBounds");
    if (absTol <= 0)
        throw Error("integral: absTol must be positive",
                     0, 0, "integral", "", "m:integral:badTol");
    const double sign = (b < a) ? -1.0 : 1.0;
    if (b < a) std::swap(a, b);
    if (a == b) return Value::scalar(0.0, mr);
    constexpr int kMaxDepth = 20;
    const double r = adaptiveIntegral(fn, a, b, absTol, 0, kMaxDepth);
    return Value::scalar(sign * r, mr);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

void gradient_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gradient: requires at least 1 argument",
                     0, 0, "gradient", "", "m:gradient:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();

    double hx = 1.0, hy = 1.0;
    if (args.size() >= 2) hx = args[1].toScalar();
    if (args.size() >= 3) hy = args[2].toScalar();
    else                  hy = hx;  // single spacing applies to both axes

    if (nargout <= 1) {
        outs[0] = gradient(args[0], hx, mr);
        return;
    }
    auto [fx, fy] = gradient2(args[0], hx, hy, mr);
    outs[0] = std::move(fx);
    outs[1] = std::move(fy);
}

void cumtrapz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumtrapz: requires at least 1 argument",
                     0, 0, "cumtrapz", "", "m:cumtrapz:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = cumtrapz(args[0], mr);
        return;
    }
    outs[0] = cumtrapz(args[0], args[1], mr);
}

void integral_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("integral: requires at least 3 arguments (fn, a, b)",
                     0, 0, "integral", "", "m:integral:nargin");
    if (!args[0].isFuncHandle()
        && !(args[0].isCell() && args[0].numel() >= 1
             && args[0].cellAt(0).isFuncHandle()))
        throw Error("integral: 1st argument must be a function handle",
                     0, 0, "integral", "", "m:integral:fnType");
    const double a = args[1].toScalar();
    const double b = args[2].toScalar();
    double absTol = 1e-10;
    for (size_t i = 3; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("integral: expected option name (string)",
                         0, 0, "integral", "", "m:integral:badFlag");
        std::string key = args[i].toString();
        for (auto &c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (key == "abstol") {
            absTol = args[i + 1].toScalar();
        } else {
            throw Error("integral: unsupported option '" + key + "'",
                         0, 0, "integral", "", "m:integral:badFlag");
        }
    }
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = integral(cb, a, b, absTol, ctx.engine->resource());
}

void trapz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("trapz: requires at least 1 argument",
                     0, 0, "trapz", "", "m:trapz:nargin");
    if (args.size() == 1)
        outs[0] = trapz(args[0], ctx.engine->resource());
    else
        outs[0] = trapz(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

// ════════════════════════════════════════════════════════════════════════
// trapz (moved from libs/fit) — uniform-spacing trapezoidal integration.
// ════════════════════════════════════════════════════════════════════════

Value trapz(const Value &y, std::pmr::memory_resource *mr)
{
    const double *yd = y.doubleData();
    const size_t n = y.numel();
    double s = 0.0;
    for (size_t i = 1; i < n; ++i)
        s += 0.5 * (yd[i - 1] + yd[i]);
    return Value::scalar(s, mr);
}

Value trapz(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (y.numel() != n)
        throw Error("trapz: x and y must have same length",
                     0, 0, "trapz", "", "m:trapz:lengthMismatch");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    double s = 0.0;
    for (size_t i = 1; i < n; ++i)
        s += 0.5 * (yd[i - 1] + yd[i]) * (xd[i] - xd[i - 1]);
    return Value::scalar(s, mr);
}

} // namespace numkit::builtin
