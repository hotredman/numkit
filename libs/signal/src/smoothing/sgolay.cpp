// libs/dsp/src/MDspSgolay.cpp

#include <numkit/m/signal/smoothing/sgolay.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>
#include <cstring>
#include <vector>

namespace numkit::m::signal {

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
            throw MError("sgolay: singular normal equations "
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
std::vector<double> buildVandermonde(int order, int framelen)
{
    const int n = framelen;
    const int p = order + 1;
    std::vector<double> V(n * p, 0.0);
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
std::vector<double> buildProjection(int order, int framelen)
{
    const int n = framelen;
    const int p = order + 1;
    auto V = buildVandermonde(order, framelen);          // n × p

    // Form V' · V  (p × p) and V'  (p × n) on the side.
    std::vector<double> VtV(p * p, 0.0);
    std::vector<double> Vt (p * n, 0.0);
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
    std::vector<double> X = Vt;
    gaussJordan(VtV.data(), X.data(), p, n);

    std::vector<double> B(n * n, 0.0);
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

MValue sgolay(Allocator &alloc, int order, int framelen)
{
    if (framelen <= 0)
        throw MError("sgolay: framelen must be positive",
                     0, 0, "sgolay", "", "m:sgolay:badArg");
    if ((framelen & 1) == 0)
        throw MError("sgolay: framelen must be odd",
                     0, 0, "sgolay", "", "m:sgolay:evenFramelen");
    if (order < 0)
        throw MError("sgolay: order must be non-negative",
                     0, 0, "sgolay", "", "m:sgolay:badArg");
    if (order >= framelen)
        throw MError("sgolay: order must be less than framelen",
                     0, 0, "sgolay", "", "m:sgolay:orderTooHigh");

    auto B = buildProjection(order, framelen);
    // Convert row-major B to column-major MValue (R = framelen, C = framelen).
    auto out = MValue::matrix(framelen, framelen, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    for (int i = 0; i < framelen; ++i)
        for (int j = 0; j < framelen; ++j)
            dst[j * framelen + i] = B[i * framelen + j];
    return out;
}

MValue sgolayfilt(Allocator &alloc, const MValue &x, int order, int framelen)
{
    if (x.type() == MType::COMPLEX)
        throw MError("sgolayfilt: complex inputs are not supported",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:complex");
    if (!x.dims().isVector() && !x.isScalar())
        throw MError("sgolayfilt: input must be a vector",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:notVector");

    const int n = static_cast<int>(x.numel());
    if (n < framelen)
        throw MError("sgolayfilt: signal length must be >= framelen",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:tooShort");

    auto B = buildProjection(order, framelen);  // throws if shape invalid
    const int half = framelen / 2;

    // Source as DOUBLE.
    std::vector<double> src(n);
    for (int i = 0; i < n; ++i) src[i] = x.elemAsDouble(i);

    auto out = createLike(x, MType::DOUBLE, &alloc);
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

void sgolay_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("sgolay: requires 2 arguments (order, framelen)",
                     0, 0, "sgolay", "", "m:sgolay:nargin");
    outs[0] = sgolay(ctx.engine->allocator(),
                     static_cast<int>(args[0].toScalar()),
                     static_cast<int>(args[1].toScalar()));
}

void sgolayfilt_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("sgolayfilt: requires 3 arguments (x, order, framelen)",
                     0, 0, "sgolayfilt", "", "m:sgolayfilt:nargin");
    outs[0] = sgolayfilt(ctx.engine->allocator(), args[0],
                         static_cast<int>(args[1].toScalar()),
                         static_cast<int>(args[2].toScalar()));
}

} // namespace detail

} // namespace numkit::m::signal
