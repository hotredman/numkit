// libs/image/src/transform/transform.cpp
//
// 2-D DCT/IDCT and the dctmtx generator. Internally these compose the
// 1-D transforms from libs/signal — DCT-II is separable, so a 2-D
// transform is just two passes of 1-D DCT (rows then columns, or vice
// versa). Storage is column-major throughout, matching the rest of
// numkit's Value layout.

#include <numkit/image/transform/transform.hpp>

#include <numkit/signal/transforms/dct.hpp>

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

} // namespace detail

} // namespace numkit::image
