// libs/builtin/src/MStdCalculus.cpp

#include <numkit/m/builtin/MStdCalculus.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace numkit::m::builtin {

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
// Operates per row using stride-R access.
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
        // Endpoints.
        dst[r]               = (src[r + R]              - src[r])           * invH;
        dst[r + (C - 1) * R] = (src[r + (C - 1) * R]    - src[r + (C - 2) * R]) * invH;
        // Interior.
        for (size_t c = 1; c + 1 < C; ++c)
            dst[r + c * R] = (src[r + (c + 1) * R] - src[r + (c - 1) * R]) * inv2h;
    }
}

// 2D gradient along dim-1 (rows) for a matrix in column-major layout.
// Per column, contiguous along the source rows — falls through to
// the same pattern as gradient1D applied per column slab.
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

MValue toDoubleCopy(Allocator &alloc, const MValue &x)
{
    auto r = createLike(x, MType::DOUBLE, &alloc);
    if (x.type() == MType::DOUBLE) {
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

MValue gradient(Allocator &alloc, const MValue &f, double h)
{
    if (h <= 0)
        throw MError("gradient: spacing h must be positive",
                     0, 0, "gradient", "", "m:gradient:badSpacing");
    if (f.type() == MType::COMPLEX)
        throw MError("gradient: complex inputs are not supported",
                     0, 0, "gradient", "", "m:gradient:complex");

    auto src = toDoubleCopy(alloc, f);
    auto out = createLike(f, MType::DOUBLE, &alloc);
    const auto &d = f.dims();

    if (f.dims().isVector() || f.isScalar()) {
        gradient1D(src.doubleData(), out.doubleDataMut(), f.numel(), h);
        return out;
    }
    if (d.is3D() || d.ndim() > 2)
        throw MError("gradient: only 1D vector and 2D matrix inputs are supported",
                     0, 0, "gradient", "", "m:gradient:rank");
    // 2D matrix: ∂F/∂x along dim-2 (columns).
    gradientAlongCols(src.doubleData(), out.doubleDataMut(),
                      d.rows(), d.cols(), h);
    return out;
}

std::tuple<MValue, MValue>
gradient2(Allocator &alloc, const MValue &f, double hx, double hy)
{
    if (hx <= 0 || hy <= 0)
        throw MError("gradient: spacing arguments must be positive",
                     0, 0, "gradient", "", "m:gradient:badSpacing");
    if (f.type() == MType::COMPLEX)
        throw MError("gradient: complex inputs are not supported",
                     0, 0, "gradient", "", "m:gradient:complex");
    const auto &d = f.dims();
    if (d.is3D() || d.ndim() > 2)
        throw MError("gradient: 2-output form requires a 2D matrix input",
                     0, 0, "gradient", "", "m:gradient:rank");

    auto src = toDoubleCopy(alloc, f);
    auto fx = createLike(f, MType::DOUBLE, &alloc);
    auto fy = createLike(f, MType::DOUBLE, &alloc);

    // For a 1-D row/column vector, both directional gradients are the
    // 1-D gradient itself (hx in row-vector case, hy in column-vector).
    if (f.dims().isVector() || f.isScalar()) {
        // Use hx for x-direction and hy for y-direction; for a row
        // vector y-direction is degenerate (length 1) → all zeros.
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

MValue cumtrapzVector(Allocator &alloc, const double *y, const double *x,
                      size_t n, const Dims &shape, bool unitSpacing)
{
    auto out = MValue::matrix(shape.rows(), shape.cols(), MType::DOUBLE, &alloc);
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

MValue cumtrapz(Allocator &alloc, const MValue &y)
{
    if (y.type() == MType::COMPLEX)
        throw MError("cumtrapz: complex inputs are not supported",
                     0, 0, "cumtrapz", "", "m:cumtrapz:complex");
    if (!y.dims().isVector() && !y.isScalar())
        throw MError("cumtrapz: matrix inputs are not yet supported "
                     "(use vector input for now)",
                     0, 0, "cumtrapz", "", "m:cumtrapz:matrixUnsupported");

    auto ys = toDoubleCopy(alloc, y);
    return cumtrapzVector(alloc, ys.doubleData(), nullptr, y.numel(),
                          y.dims(), /*unitSpacing=*/true);
}

MValue cumtrapz(Allocator &alloc, const MValue &x, const MValue &y)
{
    if (x.type() == MType::COMPLEX || y.type() == MType::COMPLEX)
        throw MError("cumtrapz: complex inputs are not supported",
                     0, 0, "cumtrapz", "", "m:cumtrapz:complex");
    if (!x.dims().isVector() || !y.dims().isVector())
        throw MError("cumtrapz: matrix inputs are not yet supported",
                     0, 0, "cumtrapz", "", "m:cumtrapz:matrixUnsupported");
    if (x.numel() != y.numel())
        throw MError("cumtrapz: x and y must have the same length",
                     0, 0, "cumtrapz", "", "m:cumtrapz:lengthMismatch");

    auto xs = toDoubleCopy(alloc, x);
    auto ys = toDoubleCopy(alloc, y);
    return cumtrapzVector(alloc, ys.doubleData(), xs.doubleData(),
                          y.numel(), y.dims(), /*unitSpacing=*/false);
}

// ── fzero (Brent's method) ───────────────────────────────────────────
namespace {

double evalCallback(Engine *engine, const MValue &fn, double x)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue arg = MValue::scalar(x, &alloc);
    Span<const MValue> args(&arg, 1);
    MValue r = engine->callFunctionHandle(fn, args);
    if (!r.isScalar() && r.numel() != 1)
        throw MError("fzero: handle must return a scalar value",
                     0, 0, "fzero", "", "m:fzero:nonScalar");
    return r.elemAsDouble(0);
}

// Expand a bracket around x0 by stepping outward by an increasing
// factor until a sign change is detected. Throws if not found within
// kMaxExpansions iterations.
std::pair<double, double>
findBracket(Engine *engine, const MValue &fn, double x0)
{
    constexpr int kMaxExpansions = 60;
    double step = (x0 == 0.0) ? 0.02 : std::abs(x0) * 0.02;
    if (step == 0.0) step = 0.02;
    double a = x0, b = x0;
    double fa = evalCallback(engine, fn, a);
    if (fa == 0.0) return {a, a};
    double fb = fa;
    for (int i = 0; i < kMaxExpansions; ++i) {
        const double s = step * std::pow(2.0, i);
        const double aPrev = a, fAprev = fa;
        a = x0 - s;
        b = x0 + s;
        fa = evalCallback(engine, fn, a);
        if (fa == 0.0) return {a, a};
        fb = evalCallback(engine, fn, b);
        if (fb == 0.0) return {b, b};
        if ((fa < 0) != (fb < 0)) return {a, b};
        // Also check the half-step for tighter brackets (a, x0).
        if ((fAprev < 0) != (fa < 0)) return {a, aPrev};
    }
    throw MError("fzero: failed to find a bracket containing a sign change "
                 "near x0",
                 0, 0, "fzero", "", "m:fzero:noBracket");
}

// Brent's method on [a, b] with f(a)*f(b) < 0 (or one of them == 0).
// Returns the root.
double brent(Engine *engine, const MValue &fn, double a, double b)
{
    constexpr int    kMaxIter = 200;
    constexpr double kEps     = 1e-15;

    double fa = evalCallback(engine, fn, a);
    double fb = evalCallback(engine, fn, b);
    if (fa == 0.0) return a;
    if (fb == 0.0) return b;
    if ((fa < 0) == (fb < 0))
        throw MError("fzero: f(a) and f(b) must have opposite signs "
                     "(no sign change in the supplied interval)",
                     0, 0, "fzero", "", "m:fzero:noSignChange");

    double c = a, fc = fa, d = b - a, e = d;
    for (int it = 0; it < kMaxIter; ++it) {
        if ((fb < 0) == (fc < 0)) {
            c = a; fc = fa; d = b - a; e = d;
        }
        if (std::abs(fc) < std::abs(fb)) {
            a = b;  b = c;  c = a;
            fa = fb; fb = fc; fc = fa;
        }
        const double tol1 = 2.0 * kEps * std::abs(b) + 0.5 * 1e-15;
        const double xm   = 0.5 * (c - b);
        if (std::abs(xm) <= tol1 || fb == 0.0) return b;

        if (std::abs(e) >= tol1 && std::abs(fa) > std::abs(fb)) {
            const double s = fb / fa;
            double p, q;
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                const double r = fb / fc;
                const double sa = fa / fc;
                p = s * (2.0 * xm * sa * (sa - r) - (b - a) * (r - 1.0));
                q = (sa - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0) q = -q;
            p = std::abs(p);
            const double min1 = 3.0 * xm * q - std::abs(tol1 * q);
            const double min2 = std::abs(e * q);
            if (2.0 * p < std::min(min1, min2)) {
                e = d;
                d = p / q;
            } else {
                d = xm; e = d;
            }
        } else {
            d = xm; e = d;
        }
        a = b; fa = fb;
        if (std::abs(d) > tol1)
            b += d;
        else
            b += (xm > 0 ? std::abs(tol1) : -std::abs(tol1));
        fb = evalCallback(engine, fn, b);
    }
    throw MError("fzero: failed to converge within iteration limit",
                 0, 0, "fzero", "", "m:fzero:noConverge");
}

} // namespace

MValue fzero(Allocator &alloc, const MValue &fn, const MValue &x0OrInterval,
             Engine *engine)
{
    if (engine == nullptr)
        throw MError("fzero: requires an Engine pointer (callback API)",
                     0, 0, "fzero", "", "m:fzero:noEngine");
    if (!fn.isFuncHandle()
        && !(fn.isCell() && fn.numel() >= 1 && fn.cellAt(0).isFuncHandle()))
        throw MError("fzero: 1st argument must be a function handle",
                     0, 0, "fzero", "", "m:fzero:fnType");

    // Bracket form: [a, b].
    if (x0OrInterval.numel() == 2) {
        const double a = x0OrInterval.elemAsDouble(0);
        const double b = x0OrInterval.elemAsDouble(1);
        if (!std::isfinite(a) || !std::isfinite(b) || a >= b)
            throw MError("fzero: interval [a, b] must satisfy a < b and be finite",
                         0, 0, "fzero", "", "m:fzero:badInterval");
        return MValue::scalar(brent(engine, fn, a, b), &alloc);
    }

    // Scalar x0 form: search outward for a bracket, then Brent.
    if (!x0OrInterval.isScalar())
        throw MError("fzero: 2nd argument must be a scalar x0 or a 2-element "
                     "interval",
                     0, 0, "fzero", "", "m:fzero:badX0");
    const double x0 = x0OrInterval.toScalar();
    if (!std::isfinite(x0))
        throw MError("fzero: x0 must be finite",
                     0, 0, "fzero", "", "m:fzero:badX0");
    auto [a, b] = findBracket(engine, fn, x0);
    if (a == b) return MValue::scalar(a, &alloc);
    if (a > b) std::swap(a, b);
    return MValue::scalar(brent(engine, fn, a, b), &alloc);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

void gradient_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("gradient: requires at least 1 argument",
                     0, 0, "gradient", "", "m:gradient:nargin");
    Allocator &alloc = ctx.engine->allocator();

    // Spacing arg(s).
    double hx = 1.0, hy = 1.0;
    if (args.size() >= 2) hx = args[1].toScalar();
    if (args.size() >= 3) hy = args[2].toScalar();
    else                  hy = hx;  // single spacing applies to both axes

    if (nargout <= 1) {
        outs[0] = gradient(alloc, args[0], hx);
        return;
    }
    auto [fx, fy] = gradient2(alloc, args[0], hx, hy);
    outs[0] = std::move(fx);
    outs[1] = std::move(fy);
}

void cumtrapz_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("cumtrapz: requires at least 1 argument",
                     0, 0, "cumtrapz", "", "m:cumtrapz:nargin");
    Allocator &alloc = ctx.engine->allocator();
    if (args.size() == 1) {
        outs[0] = cumtrapz(alloc, args[0]);
        return;
    }
    outs[0] = cumtrapz(alloc, args[0], args[1]);
}

void fzero_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("fzero: requires at least 2 arguments (fn, x0 or [a, b])",
                     0, 0, "fzero", "", "m:fzero:nargin");
    outs[0] = fzero(ctx.engine->allocator(), args[0], args[1], ctx.engine);
}

} // namespace detail

} // namespace numkit::m::builtin
