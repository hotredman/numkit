// libs/image/src/transform/transform.cpp
//
// 2-D DCT/IDCT and the dctmtx generator. Internally these compose the
// 1-D transforms from libs/signal — DCT-II is separable, so a 2-D
// transform is just two passes of 1-D DCT (rows then columns, or vice
// versa). Storage is column-major throughout, matching the rest of
// numkit's Value layout.

#include <numkit/image/transform/transform.hpp>

#include <numkit/signal/transforms/dct.hpp>
#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::image {

namespace {

// Apply 1-D DCT (or IDCT) to every column of A and write results to
// `out`. Columns of `A` are length-M contiguous slices in column-major
// storage. We slice each column into a temporary M×1 vector, run
// signal::dct/idct on it, and copy the result back. Allocations come
// from the engine arena (mr) so per-call cost stays bounded.
template <typename Fn1D>
Value apply_along_columns(std::pmr::memory_resource *mr,
                          const Value &A, Fn1D &&fn1d)
{
    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    Value out = Value::matrix(M, N, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) return out;
    double *dst = out.doubleDataMut();

    auto col = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *cd = col.doubleDataMut();

    for (size_t c = 0; c < N; ++c) {
        for (size_t r = 0; r < M; ++r)
            cd[r] = A.elemAsDouble(c * M + r);
        Value Y = fn1d(mr, col);
        const double *yd = Y.doubleData();
        for (size_t r = 0; r < M; ++r)
            dst[c * M + r] = yd[r];
    }
    return out;
}

// Apply 1-D DCT/IDCT to every row of A. Allocates a length-N row
// buffer per row.
template <typename Fn1D>
Value apply_along_rows(std::pmr::memory_resource *mr,
                       const Value &A, Fn1D &&fn1d)
{
    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    Value out = Value::matrix(M, N, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) return out;
    double *dst = out.doubleDataMut();

    auto row = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *rd = row.doubleDataMut();

    for (size_t r = 0; r < M; ++r) {
        for (size_t c = 0; c < N; ++c)
            rd[c] = A.elemAsDouble(c * M + r);
        Value Y = fn1d(mr, row);
        const double *yd = Y.doubleData();
        for (size_t c = 0; c < N; ++c)
            dst[c * M + r] = yd[c];
    }
    return out;
}

} // anonymous

Value dct2(std::pmr::memory_resource *mr, const Value &A)
{
    // Two passes of orthonormal Type-II DCT (separable). Columns first,
    // then rows — output is identical either way.
    Value Y = apply_along_columns(mr, A, &numkit::signal::dct);
    return apply_along_rows(mr, Y, &numkit::signal::dct);
}

Value idct2(std::pmr::memory_resource *mr, const Value &A)
{
    Value Y = apply_along_columns(mr, A, &numkit::signal::idct);
    return apply_along_rows(mr, Y, &numkit::signal::idct);
}

Value dctmtx(std::pmr::memory_resource *mr, double Nd)
{
    // MATLAB's dctmtx(N) returns an N×N matrix D whose rows are the
    // DCT-II basis vectors:
    //   D[k, n] = w[k] · cos(π · (2n+1) · k / (2N))
    //   w[0]    = sqrt(1/N),  w[k>0] = sqrt(2/N)
    // so D*x is the DCT-II of x.
    if (!(Nd > 0.0) || std::floor(Nd) != Nd)
        throw Error("dctmtx: N must be a positive integer",
                    0, 0, "dctmtx", "", "m:dctmtx:arg");
    const size_t N = static_cast<size_t>(Nd);
    Value D = Value::matrix(N, N, ValueType::DOUBLE, mr);
    if (N == 0) return D;
    double *d = D.doubleDataMut();
    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));
    for (size_t n = 0; n < N; ++n) {
        for (size_t k = 0; k < N; ++k) {
            const double phase = piOver2N * static_cast<double>(k)
                                 * static_cast<double>(2 * n + 1);
            const double w = (k == 0) ? w0 : wk;
            // Column-major: element (k, n) at offset n*N + k.
            d[n * N + k] = w * std::cos(phase);
        }
    }
    return D;
}

Value integralImage(std::pmr::memory_resource *mr, const Value &I)
{
    const auto &d = I.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t H1 = H + 1;
    Value out = Value::matrix(H1, W + 1, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;
    double *od = out.doubleDataMut();
    // out[r+1, c+1] = out[r, c+1] + out[r+1, c] - out[r, c] + I[r, c].
    for (size_t c = 0; c < W; ++c) {
        const size_t cb = (c + 1) * H1;
        const size_t cl = c * H1;
        for (size_t r = 0; r < H; ++r) {
            const size_t r1 = r + 1;
            od[cb + r1] = od[cl + r1] + od[cb + r] - od[cl + r]
                        + I.elemAsDouble(c * H + r);
        }
    }
    return out;
}

Value integralImage3(std::pmr::memory_resource *mr, const Value &V)
{
    const auto &d = V.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1;
    const size_t H1 = H + 1;
    const size_t W1 = W + 1;
    const size_t plane1 = H1 * W1;
    const size_t planeIn = H * W;
    Value out = Value::matrix3d(H1, W1, P + 1, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0 || P == 0) return out;
    double *od = out.doubleDataMut();
    auto idx = [&](size_t r, size_t c, size_t p) {
        return p * plane1 + c * H1 + r;
    };
    for (size_t p = 0; p < P; ++p) {
        const size_t p1 = p + 1;
        for (size_t c = 0; c < W; ++c) {
            const size_t c1 = c + 1;
            for (size_t r = 0; r < H; ++r) {
                const size_t r1 = r + 1;
                const double v = V.elemAsDouble(p * planeIn + c * H + r);
                od[idx(r1, c1, p1)] =
                      od[idx(r,  c1, p1)]
                    + od[idx(r1, c,  p1)]
                    + od[idx(r1, c1, p )]
                    - od[idx(r,  c,  p1)]
                    - od[idx(r,  c1, p )]
                    - od[idx(r1, c,  p )]
                    + od[idx(r,  c,  p )]
                    + v;
            }
        }
    }
    return out;
}

Value normxcorr2(std::pmr::memory_resource *mr,
                 const Value &templ, const Value &img)
{
    const size_t mH = templ.dims().rows();
    const size_t mW = templ.dims().cols();
    const size_t bH = img.dims().rows();
    const size_t bW = img.dims().cols();
    const size_t mN = mH * mW;

    // Build double-centered template (a) and image (b).
    Value a = Value::matrix(mH, mW, ValueType::DOUBLE, mr);
    Value b = Value::matrix(bH, bW, ValueType::DOUBLE, mr);
    double *ad = a.doubleDataMut();
    double *bd = b.doubleDataMut();

    long double sa = 0.0L, sb = 0.0L;
    for (size_t i = 0; i < mN; ++i) sa += templ.elemAsDouble(i);
    for (size_t i = 0; i < bH * bW; ++i) sb += img.elemAsDouble(i);
    const double ma = (mN     > 0) ? static_cast<double>(sa / static_cast<long double>(mN))     : 0.0;
    const double mb = (bH * bW > 0) ? static_cast<double>(sb / static_cast<long double>(bH * bW)) : 0.0;
    for (size_t i = 0; i < mN; ++i)     ad[i] = templ.elemAsDouble(i) - ma;
    for (size_t i = 0; i < bH * bW; ++i) bd[i] = img.elemAsDouble(i)  - mb;

    // Reversed template ar = rot180(a). Column-major: ar[(mW-1-c)*mH + (mH-1-r)] = a[c*mH + r].
    Value ar = Value::matrix(mH, mW, ValueType::DOUBLE, mr);
    double *ard = ar.doubleDataMut();
    for (size_t c = 0; c < mW; ++c)
        for (size_t r = 0; r < mH; ++r)
            ard[(mW - 1 - c) * mH + (mH - 1 - r)] = ad[c * mH + r];

    // Numerator: conv2(b, ar, 'full').
    Value c_num = signal::conv2(mr, b, ar, "full");

    // Denominator pieces use a1 = ones(size(a)).
    Value a1 = Value::matrix(mH, mW, ValueType::DOUBLE, mr);
    double *a1d = a1.doubleDataMut();
    for (size_t i = 0; i < mN; ++i) a1d[i] = 1.0;

    // b_sq = b .^ 2.
    Value b_sq = Value::matrix(bH, bW, ValueType::DOUBLE, mr);
    double *bsd = b_sq.doubleDataMut();
    for (size_t i = 0; i < bH * bW; ++i) bsd[i] = bd[i] * bd[i];

    Value sum_b_sq = signal::conv2(mr, b_sq, a1, "full");
    Value sum_b    = signal::conv2(mr, b,    a1, "full");

    // c_denom = sum_b_sq - sum_b.^2 / mN (clamped at 0).
    const size_t outH = bH + mH - 1;
    const size_t outW = bW + mW - 1;
    Value c_denom = Value::matrix(outH, outW, ValueType::DOUBLE, mr);
    double *cdd = c_denom.doubleDataMut();
    const double *sbsd = sum_b_sq.doubleData();
    const double *sbd  = sum_b.doubleData();
    for (size_t i = 0; i < outH * outW; ++i) {
        double v = sbsd[i] - (sbd[i] * sbd[i]) /
                              static_cast<double>(mN > 0 ? mN : 1);
        if (v < 0.0) v = 0.0;
        cdd[i] = v;
    }

    // sumsq(a).
    long double sumsq_a = 0.0L;
    for (size_t i = 0; i < mN; ++i) sumsq_a += static_cast<long double>(ad[i]) * ad[i];
    const double sa2 = static_cast<double>(sumsq_a);

    // c = c_num / sqrt(c_denom * sumsq_a); inf/nan → 0.
    Value c_out = Value::matrix(outH, outW, ValueType::DOUBLE, mr);
    double *cod = c_out.doubleDataMut();
    const double *cnd = c_num.doubleData();
    for (size_t i = 0; i < outH * outW; ++i) {
        const double denom = std::sqrt(cdd[i] * sa2);
        double v = (denom > 0.0) ? cnd[i] / denom : 0.0;
        if (!std::isfinite(v)) v = 0.0;
        cod[i] = v;
    }
    return c_out;
}

Value checkerboard(std::pmr::memory_resource *mr,
                   size_t side, size_t M, size_t N)
{
    const size_t H = 2 * M * side;
    const size_t W = 2 * N * side;
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0 || side == 0) return out;
    double *od = out.doubleDataMut();

    // Tile pattern: 2*side × 2*side. Build via linspace(-1, 1, 2*side):
    //   x[i] = -1 + 2 * i / (2*side - 1)
    // tile(r, c) = (x[r] * x[c]) < 0  → 1.0 in opposite-sign quadrants.
    const size_t S2 = 2 * side;
    std::vector<double> x(S2);
    if (S2 == 1) x[0] = -1.0;
    else
        for (size_t i = 0; i < S2; ++i)
            x[i] = -1.0 + 2.0 * static_cast<double>(i)
                          / static_cast<double>(S2 - 1);

    // Right half (cols ≥ W/2) is dimmed to 0.7.
    const size_t halfW = W / 2;
    for (size_t c = 0; c < W; ++c) {
        const double xc = x[c % S2];
        const double dim = (c >= halfW) ? 0.7 : 1.0;
        for (size_t r = 0; r < H; ++r) {
            const double xr = x[r % S2];
            const double tile = (xr * xc < 0.0) ? 1.0 : 0.0;
            od[c * H + r] = tile * dim;
        }
    }
    return out;
}

namespace detail {

void integralImage_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralImage: requires (I)",
                    0, 0, "integralImage", "", "m:integralImage:nargin");
    outs[0] = integralImage(ctx.engine->resource(), args[0]);
}

void integralImage3_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralImage3: requires (V)",
                    0, 0, "integralImage3", "", "m:integralImage3:nargin");
    outs[0] = integralImage3(ctx.engine->resource(), args[0]);
}

void dct2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("dct2: requires 1 argument",
                    0, 0, "dct2", "", "m:dct2:nargin");
    outs[0] = dct2(ctx.engine->resource(), args[0]);
}

void idct2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("idct2: requires 1 argument",
                    0, 0, "idct2", "", "m:idct2:nargin");
    outs[0] = idct2(ctx.engine->resource(), args[0]);
}

void dctmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("dctmtx: requires 1 argument (N)",
                    0, 0, "dctmtx", "", "m:dctmtx:nargin");
    outs[0] = dctmtx(ctx.engine->resource(), args[0].toScalar());
}

void normxcorr2_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("normxcorr2: requires (template, img)",
                    0, 0, "normxcorr2", "", "m:normxcorr2:nargin");
    outs[0] = normxcorr2(ctx.engine->resource(), args[0], args[1]);
}

void checkerboard_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    size_t side = 10, M = 4, N = 4;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const double s = args[0].toScalar();
        if (s < 0.0 || s != std::floor(s))
            throw Error("checkerboard: SIDE must be a non-negative integer",
                        0, 0, "checkerboard", "", "m:checkerboard:side");
        side = static_cast<size_t>(s);
    }
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) { M = N = static_cast<size_t>(v.toScalar()); }
        else if (v.numel() >= 2) {
            M = static_cast<size_t>(v.elemAsDouble(0));
            N = static_cast<size_t>(v.elemAsDouble(1));
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        N = static_cast<size_t>(args[2].toScalar());
    outs[0] = checkerboard(ctx.engine->resource(), side, M, N);
}

} // namespace detail

} // namespace numkit::image
