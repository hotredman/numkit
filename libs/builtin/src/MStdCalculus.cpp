// libs/builtin/src/MStdCalculus.cpp

#include <numkit/m/builtin/MStdCalculus.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cstring>
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

} // namespace detail

} // namespace numkit::m::builtin
