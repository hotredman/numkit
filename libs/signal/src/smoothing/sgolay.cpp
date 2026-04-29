// libs/signal/src/smoothing/sgolay.cpp

#include <numkit/signal/smoothing/sgolay.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <cstring>
#include <memory_resource>

namespace numkit::signal {

namespace {

// Solve A · X = B in place (Gauss-Jordan with partial pivoting).
// A is N×N row-major, B is N×M row-major.
void gaussJordan(double *A, double *B, int N, int M)
{
    for (int k = 0; k < N; ++k) {
        // Pivot.
        int piv = k;
        double maxAbs = std::abs(A[k * N + k]);
        for (int r = k + 1; r < N; ++r) {
            const double v = std::abs(A[r * N + k]);
            if (v > maxAbs) { maxAbs = v; piv = r; }
        }
        if (maxAbs < 1e-300)
            throw Error("sgolay: singular normal equations "
                         "(framelen too small for order)",
                         0, 0, "sgolay", "", "m:sgolay:singular");
        if (piv != k) {
            for (int c = 0; c < N; ++c) std::swap(A[k * N + c], A[piv * N + c]);
            for (int c = 0; c < M; ++c) std::swap(B[k * M + c], B[piv * M + c]);
        }
        // Normalise pivot row.
        const double inv = 1.0 / A[k * N + k];
        for (int c = 0; c < N; ++c) A[k * N + c] *= inv;
        for (int c = 0; c < M; ++c) B[k * M + c] *= inv;
        // Eliminate.
        for (int r = 0; r < N; ++r) {
            if (r == k) continue;
            const double f = A[r * N + k];
            if (f == 0.0) continue;
            for (int c = 0; c < N; ++c) A[r * N + c] -= f * A[k * N + c];
            for (int c = 0; c < M; ++c) B[r * M + c] -= f * B[k * M + c];
        }
    }
}

// Build the (framelen × (order+1)) Vandermonde matrix V where
// V[i, k] = (i - center)^k for i = 0..framelen-1 and k = 0..order.
ScratchVec<double> buildVandermonde(std::pmr::memory_resource *mr,
                                    int order, int framelen)
{
    const int n = framelen;
    const int p = order + 1;
    ScratchVec<double> V(static_cast<std::size_t>(n * p), mr);
    const double half = static_cast<double>(framelen / 2);
    for (int i = 0; i < n; ++i) {
        double v = 1.0;
        const double x = static_cast<double>(i) - half;
        for (int k = 0; k < p; ++k) {
            V[i * p + k] = v;
            v *= x;
        }
    }
    return V;
}

// Compute B = V · (V' · V)^-1 · V'   (the framelen × framelen
// projection matrix). Each row r of B gives the filter coefficients
// for sample r in the window: y_r = B[r, :] · x_window.
ScratchVec<double> buildProjection(std::pmr::memory_resource *mr,
                                   int order, int framelen)
{
    const int n = framelen;
    const int p = order + 1;
    auto V = buildVandermonde(mr, order, framelen);      // n × p

    // Form V' · V  (p × p) and V'  (p × n) on the side.
    ScratchVec<double> VtV(static_cast<std::size_t>(p * p), mr);
    ScratchVec<double> Vt (static_cast<std::size_t>(p * n), mr);
    for (int k = 0; k < p; ++k)
        for (int i = 0; i < n; ++i)
            Vt[k * n + i] = V[i * p + k];
    for (int i = 0; i < p; ++i)
        for (int j = 0; j < p; ++j) {
            double s = 0.0;
            for (int t = 0; t < n; ++t)
                s += V[t * p + i] * V[t * p + j];
            VtV[i * p + j] = s;
        }

    // Solve VtV · X = Vt → X is p × n; then B = V · X (n × n).
    ScratchVec<double> X(Vt, mr);
    gaussJordan(VtV.data(), X.data(), p, n);

    ScratchVec<double> B(static_cast<std::size_t>(n * n), mr);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            double s = 0.0;
            for (int k = 0; k < p; ++k)
                s += V[i * p + k] * X[k * n + j];
            B[i * n + j] = s;
        }
    return B;
}

} // namespace

Value sgolay(std::pmr::memory_resource *mr, int order, int framelen)
{
    if (framelen <= 0)
        throw Error("sgolay: framelen must be positive",
                     0, 0, "sgolay", "", "m:sgolay:badArg");
    if ((framelen & 1) == 0)
        throw Error("sgolay: framelen must be odd",
                     0, 0, "sgolay", "", "m:sgolay:evenFramelen");
    if (order < 0)
        throw Error("sgolay: order must be non-negative",
                     0, 0, "sgolay", "", "m:sgolay:badArg");
    if (order >= framelen)
        throw Error("sgolay: order must be less than framelen",
                     0, 0, "sgolay", "", "m:sgolay:orderTooHigh");

    ScratchArena scratch(mr);
    auto B = buildProjection(&scratch, order, framelen);
    // Convert row-major B to column-major Value (R = framelen, C = framelen).
    auto out = Value::matrix(framelen, framelen, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (int i = 0; i < framelen; ++i)
        for (int j = 0; j < framelen; ++j)
            dst[j * framelen + i] = B[i * framelen + j];
    return out;
}

Value sgolayfilt(std::pmr::memory_resource *mr, const Value &x, int order, int framelen)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error("sgolayfilt: complex inputs are not supported",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:complex");
    if (!x.dims().isVector() && !x.isScalar())
        throw Error("sgolayfilt: input must be a vector",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:notVector");

    const int n = static_cast<int>(x.numel());
    if (n < framelen)
        throw Error("sgolayfilt: signal length must be >= framelen",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:tooShort");

    ScratchArena scratch(mr);
    auto B = buildProjection(&scratch, order, framelen);  // throws if shape invalid
    const int half = framelen / 2;

    // Source as DOUBLE.
    auto src = ScratchVec<double>(static_cast<std::size_t>(n), &scratch);
    for (int i = 0; i < n; ++i) src[i] = x.elemAsDouble(i);

    auto out = createLike(x, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();

    // Interior: convolution with the central row of B.
    const double *Bcenter = &B[half * framelen];
    for (int i = half; i < n - half; ++i) {
        double s = 0.0;
        for (int k = 0; k < framelen; ++k)
            s += Bcenter[k] * src[i - half + k];
        dst[i] = s;
    }
    // Leading edge (i = 0..half-1): use row i of B applied to the
    // first framelen samples.
    for (int i = 0; i < half; ++i) {
        const double *Brow = &B[i * framelen];
        double s = 0.0;
        for (int k = 0; k < framelen; ++k)
            s += Brow[k] * src[k];
        dst[i] = s;
    }
    // Trailing edge (i = n - half..n - 1): row (framelen - (n - i))
    // of B applied to the last framelen samples.
    for (int i = n - half; i < n; ++i) {
        const int rowIdx = framelen - 1 - (n - 1 - i);
        const double *Brow = &B[rowIdx * framelen];
        double s = 0.0;
        for (int k = 0; k < framelen; ++k)
            s += Brow[k] * src[n - framelen + k];
        dst[i] = s;
    }
    return out;
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

void sgolay_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sgolay: requires 2 arguments (order, framelen)",
                     0, 0, "sgolay", "", "m:sgolay:nargin");
    outs[0] = sgolay(ctx.engine->resource(),
                     static_cast<int>(args[0].toScalar()),
                     static_cast<int>(args[1].toScalar()));
}

void sgolayfilt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("sgolayfilt: requires 3 arguments (x, order, framelen)",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:nargin");
    outs[0] = sgolayfilt(ctx.engine->resource(), args[0],
                         static_cast<int>(args[1].toScalar()),
                         static_cast<int>(args[2].toScalar()));
}

} // namespace detail

} // namespace numkit::signal
