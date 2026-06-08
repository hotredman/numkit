// toolboxes/signal/src/smoothing/sgolay.cpp

#include <numkit/signal/smoothing/sgolay.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

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
                         0, 0, "sgolay", "", "numkit:sgolay:singular");
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
ScratchVec<double> buildVandermonde(int order, int framelen, std::pmr::memory_resource *mr)
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

// Compute B = V · (V' · W · V)^-1 · V' · W   (the framelen × framelen
// projection matrix), where W = diag(w) is the optional weighting. Each
// row r of B gives the filter coefficients for sample r in the window:
// y_r = B[r, :] · x_window. `w` is nullptr (unweighted, W = I) or a
// framelen-length vector of positive weights.
ScratchVec<double> buildProjection(int order, int framelen,
                                   std::pmr::memory_resource *mr,
                                   const double *w = nullptr)
{
    const int n = framelen;
    const int p = order + 1;
    auto V = buildVandermonde(order, framelen, mr);      // n × p

    // Form V'·W·V  (p × p) and V'·W  (p × n) on the side. With w == nullptr
    // the weight factor is 1.0 and this reduces to the ordinary V'V / V'.
    ScratchVec<double> VtV(static_cast<std::size_t>(p * p), mr);
    ScratchVec<double> Vt (static_cast<std::size_t>(p * n), mr);
    for (int k = 0; k < p; ++k)
        for (int i = 0; i < n; ++i)
            Vt[k * n + i] = V[i * p + k] * (w ? w[i] : 1.0);
    for (int i = 0; i < p; ++i)
        for (int j = 0; j < p; ++j) {
            double s = 0.0;
            for (int t = 0; t < n; ++t)
                s += V[t * p + i] * (w ? w[t] : 1.0) * V[t * p + j];
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

// Compute G = V · (V'·V)^-1  (the framelen × (order+1) differentiation-
// filter matrix, row-major). Column j of G is the FIR filter that estimates
// the polynomial coefficient a_j (a_1 = value, a_2 = 1st-deriv term, …) at
// the central point. This is the unweighted second output of MATLAB's
// [B,G] = sgolay(order,framelen).
ScratchVec<double> buildDiffMatrix(int order, int framelen, std::pmr::memory_resource *mr)
{
    const int n = framelen;
    const int p = order + 1;
    auto V = buildVandermonde(order, framelen, mr);      // n × p

    // VtV = V'·V (p × p).
    ScratchVec<double> VtV(static_cast<std::size_t>(p * p), mr);
    for (int i = 0; i < p; ++i)
        for (int j = 0; j < p; ++j) {
            double s = 0.0;
            for (int t = 0; t < n; ++t)
                s += V[t * p + i] * V[t * p + j];
            VtV[i * p + j] = s;
        }

    // Invert VtV: solve VtV · inv = I.
    ScratchVec<double> inv(static_cast<std::size_t>(p * p), mr);
    for (int i = 0; i < p; ++i)
        for (int j = 0; j < p; ++j)
            inv[i * p + j] = (i == j) ? 1.0 : 0.0;
    gaussJordan(VtV.data(), inv.data(), p, p);           // inv := (V'V)^-1

    // G = V · inv (n × p).
    ScratchVec<double> G(static_cast<std::size_t>(n * p), mr);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < p; ++j) {
            double s = 0.0;
            for (int k = 0; k < p; ++k)
                s += V[i * p + k] * inv[k * p + j];
            G[i * p + j] = s;
        }
    return G;
}

// Shared validation for sgolay / sgolayDiff.
void validateSgolayArgs(int order, int framelen)
{
    if (framelen <= 0)
        throw Error("sgolay: framelen must be positive",
                     0, 0, "sgolay", "", "numkit:sgolay:badArg");
    if ((framelen & 1) == 0)
        throw Error("sgolay: framelen must be odd",
                     0, 0, "sgolay", "", "numkit:sgolay:evenFramelen");
    if (order < 0)
        throw Error("sgolay: order must be non-negative",
                     0, 0, "sgolay", "", "numkit:sgolay:badArg");
    if (order >= framelen)
        throw Error("sgolay: order must be less than framelen",
                     0, 0, "sgolay", "", "numkit:sgolay:orderTooHigh");
}

} // namespace

Value sgolay(int order, int framelen, std::pmr::memory_resource *mr)
{
    validateSgolayArgs(order, framelen);

    ScratchArena scratch(mr);
    auto B = buildProjection(order, framelen, &scratch);
    // Convert row-major B to column-major Value (R = framelen, C = framelen).
    auto out = Value::matrix(framelen, framelen, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (int i = 0; i < framelen; ++i)
        for (int j = 0; j < framelen; ++j)
            dst[j * framelen + i] = B[i * framelen + j];
    return out;
}

Value sgolayDiff(int order, int framelen, std::pmr::memory_resource *mr)
{
    validateSgolayArgs(order, framelen);

    const int p = order + 1;
    ScratchArena scratch(mr);
    auto G = buildDiffMatrix(order, framelen, &scratch);   // row-major n × p
    // Convert row-major G to column-major Value (R = framelen, C = order+1).
    auto out = Value::matrix(framelen, p, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (int i = 0; i < framelen; ++i)
        for (int j = 0; j < p; ++j)
            dst[j * framelen + i] = G[i * p + j];
    return out;
}

namespace {

// Filter one length-n 1-D slice with the framelen×framelen projection
// matrix B (row-major). Interior samples use the central (symmetric) row
// of B; the edges use the asymmetric rows so no zero-padding artefacts
// appear. n must be >= framelen.
void sgolayfiltSlice(const double *src, double *dst, int n,
                     const double *B, int framelen)
{
    const int half = framelen / 2;
    const double *Bcenter = &B[half * framelen];
    for (int i = half; i < n - half; ++i) {
        double s = 0.0;
        for (int k = 0; k < framelen; ++k)
            s += Bcenter[k] * src[i - half + k];
        dst[i] = s;
    }
    for (int i = 0; i < half; ++i) {                 // leading edge
        const double *Brow = &B[i * framelen];
        double s = 0.0;
        for (int k = 0; k < framelen; ++k)
            s += Brow[k] * src[k];
        dst[i] = s;
    }
    for (int i = n - half; i < n; ++i) {             // trailing edge
        const int rowIdx = framelen - 1 - (n - 1 - i);
        const double *Brow = &B[rowIdx * framelen];
        double s = 0.0;
        for (int k = 0; k < framelen; ++k)
            s += Brow[k] * src[n - framelen + k];
        dst[i] = s;
    }
}

// Core sgolayfilt: handles vectors and matrices along `dim` (1 or 2;
// 0 = auto = first non-singleton dimension, matching MATLAB), with an
// optional framelen-length weight vector `w` (nullptr = unweighted).
Value sgolayfiltImpl(const Value &x, int order, int framelen,
                     const double *w, int dim, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error("sgolayfilt: complex inputs are not supported",
                     0, 0, "sgolayfilt", "", "numkit:sgolayfilt:complex");
    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error("sgolayfilt: N-D (>2) inputs are not supported",
                     0, 0, "sgolayfilt", "", "numkit:sgolayfilt:ndims");

    const size_t R = d.rows(), C = d.cols();
    int useDim = dim;
    if (useDim <= 0) useDim = (R > 1) ? 1 : 2;       // first non-singleton
    if (useDim != 1 && useDim != 2)
        throw Error("sgolayfilt: dim must be 1 or 2",
                     0, 0, "sgolayfilt", "", "numkit:sgolayfilt:badDim");

    const int n = static_cast<int>(useDim == 1 ? R : C);
    if (n < framelen)
        throw Error("sgolayfilt: signal length along dim must be >= framelen",
                     0, 0, "sgolayfilt", "", "numkit:sgolayfilt:tooShort");

    ScratchArena scratch(mr);
    auto B = buildProjection(order, framelen, &scratch, w);  // throws if shape invalid

    auto out = createLike(x, ValueType::DOUBLE, mr);
    if (x.numel() == 0) return out;
    double *dst = out.doubleDataMut();

    // Gather source as DOUBLE (column-major).
    auto src = ScratchVec<double>(x.numel(), &scratch);
    for (size_t i = 0; i < x.numel(); ++i) src[i] = x.elemAsDouble(i);

    if (useDim == 1) {
        // Each column is contiguous (length R) in column-major storage.
        for (size_t c = 0; c < C; ++c)
            sgolayfiltSlice(src.data() + c * R, dst + c * R,
                            static_cast<int>(R), B.data(), framelen);
    } else {
        // Each row is strided by R: gather → filter → scatter.
        auto rbuf = ScratchVec<double>(C, &scratch);
        auto obuf = ScratchVec<double>(C, &scratch);
        for (size_t r = 0; r < R; ++r) {
            for (size_t c = 0; c < C; ++c) rbuf[c] = src[r + c * R];
            sgolayfiltSlice(rbuf.data(), obuf.data(),
                            static_cast<int>(C), B.data(), framelen);
            for (size_t c = 0; c < C; ++c) dst[r + c * R] = obuf[c];
        }
    }
    return out;
}

} // namespace

Value sgolayfilt(const Value &x, int order, int framelen, std::pmr::memory_resource *mr)
{
    return sgolayfiltImpl(x, order, framelen, nullptr, 0, mr);
}

Value sgolayfilt(const Value &x, int order, int framelen,
                 const Value &weights, int dim, std::pmr::memory_resource *mr)
{
    if (weights.isEmpty())
        return sgolayfiltImpl(x, order, framelen, nullptr, dim, mr);
    if (weights.type() == ValueType::COMPLEX)
        throw Error("sgolayfilt: weights must be real and positive",
                     0, 0, "sgolayfilt", "", "numkit:sgolayfilt:weights");
    if (static_cast<int>(weights.numel()) != framelen)
        throw Error("sgolayfilt: weights must have length framelen",
                     0, 0, "sgolayfilt", "", "numkit:sgolayfilt:weightsLen");
    ScratchArena wa(mr);
    auto w = ScratchVec<double>(static_cast<std::size_t>(framelen), &wa);
    for (int i = 0; i < framelen; ++i) {
        const double wi = weights.elemAsDouble(i);
        if (!(wi > 0.0))
            throw Error("sgolayfilt: weights must be real and positive",
                         0, 0, "sgolayfilt", "", "numkit:sgolayfilt:weights");
        w[i] = wi;
    }
    return sgolayfiltImpl(x, order, framelen, w.data(), dim, mr);
}

} // namespace numkit::signal
