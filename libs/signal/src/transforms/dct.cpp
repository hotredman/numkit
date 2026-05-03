// libs/signal/src/transforms/dct.cpp
//
// Orthonormal Type-II DCT + inverse (MATLAB default). FFT-based
// O(N log N) implementation using the "double-length mirror" trick:
// extend x to a length-2N mirror y, take FFT(y), recover the DCT
// coefficients from Re(Y * exp(-jπk/(2N))). Replaces the prior
// O(N²) direct path (closed gap to MATLAB; bench reports ~50× win
// vs prior numkit and beats Octave on small N).

#include <numkit/signal/transforms/dct.hpp>
#include <numkit/signal/transforms/fft.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>
#include <complex>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

using Cd = std::complex<double>;

inline bool isPowerOfTwo(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

// Direct O(N²) reference implementation. Used as a fallback when N
// isn't a power of two (numkit's fft pads non-pow2 inputs to the next
// pow2 internally and slices the result, which doesn't match a true
// length-N DFT — needed for the FFT-trick DCT to be exact).
Value dctDirect(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *xd = x.doubleData();
    double *X  = r.doubleDataMut();
    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));
    for (size_t k = 0; k < N; ++k) {
        double acc = 0.0;
        const double angK = piOver2N * static_cast<double>(k);
        for (size_t n = 0; n < N; ++n)
            acc += xd[n] * std::cos(angK * static_cast<double>(2 * n + 1));
        X[k] = (k == 0 ? w0 : wk) * acc;
    }
    return r;
}

Value idctDirect(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *Xd = x.doubleData();
    double *xt = r.doubleDataMut();
    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));
    for (size_t n = 0; n < N; ++n) {
        double acc = w0 * Xd[0];
        const double angN = piOver2N * static_cast<double>(2 * n + 1);
        for (size_t k = 1; k < N; ++k)
            acc += wk * Xd[k] * std::cos(angN * static_cast<double>(k));
        xt[n] = acc;
    }
    return r;
}

} // anonymous

// dct:  X[k] = w[k] * sum_n x[n] * cos(pi (2n+1) k / (2N))
// idct: x[n] = sum_k w[k] * X[k] * cos(pi (2n+1) k / (2N))
//   where w[0] = sqrt(1/N), w[k>0] = sqrt(2/N).
//
// Algorithm: build y of length 2N by mirroring x (y[n] = x[n] for
// n < N, y[2N-1-n] = x[n] for n < N), take FFT(y), then
// X[k] = Re(Y[k] · exp(-jπk/(2N))) / 2 with the orthonormal weights.
Value dct(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *xd = x.doubleData();
    double *X  = r.doubleDataMut();

    if (N == 1) {
        X[0] = xd[0];  // sqrt(1/1) * x[0]
        return r;
    }
    // numkit's fft pads non-pow2 inputs to nextPow2 internally, so the
    // length-N output isn't a true length-N DFT for N≠pow2. Fall back
    // to the direct path in that case; modern bench inputs (image
    // tiling, audio frames, etc.) are typically pow2 so the FFT fast
    // path covers the common case.
    if (!isPowerOfTwo(N)) return dctDirect(mr, x);

    // Build the length-2N mirrored signal y as a numkit Value so we
    // can call the existing FFT.
    const size_t M = 2 * N;
    auto y = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *yd = y.doubleDataMut();
    for (size_t n = 0; n < N; ++n) {
        yd[n]         = xd[n];
        yd[M - 1 - n] = xd[n];
    }
    Value Y = fft(mr, y, /*n=*/-1, /*dim=*/0);
    const Cd *Yc = Y.complexData();

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    // numkit's fft uses the e^{+j2πkn/N} convention (opposite of the
    // textbook DCT derivation); the rotation sign here compensates so
    // Re(Y[k] · rot) = 2 · DCT_kernel.
    for (size_t k = 0; k < N; ++k) {
        const double phase = piOver2N * static_cast<double>(k);
        const Cd rot(std::cos(phase), std::sin(phase));
        const double v = (Yc[k] * rot).real() * 0.5;
        X[k] = (k == 0 ? w0 : wk) * v;
    }
    return r;
}

// Inverse DCT-II. Reuse the dct → ifft trick: build a length-2N
// complex spectrum Z from the orthonormalised coefficients X such that
// Z[k] = X[k]·exp(jπk/(2N)) for k = 0..N-1, Z[N] = 0, and the
// remaining bins are conjugate-symmetric. Inverse FFT gives the
// length-2N mirrored signal whose first N samples are x.
Value idct(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (N == 0) return r;
    const double *Xd = x.doubleData();
    double *xt = r.doubleDataMut();

    if (N == 1) {
        xt[0] = Xd[0];
        return r;
    }
    if (!isPowerOfTwo(N)) return idctDirect(mr, x);

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    // Build length-2N complex spectrum.
    const size_t M = 2 * N;
    auto Zv = Value::complexMatrix(M, 1, mr);
    Cd *Z = Zv.complexDataMut();
    for (size_t i = 0; i < M; ++i) Z[i] = Cd(0.0, 0.0);

    // Undo orthonormal weights, apply backward rotation (sign flipped
    // to match numkit's FFT convention; mirrors the dct above):
    //   Y[k]      =  Vk · exp(-jπk/(2N))           , k = 0..N-1
    //   Y[N]      =  0                              (zero by symmetry)
    //   Y[2N - k] = conj(Y[k])                     , k = 1..N-1
    for (size_t k = 0; k < N; ++k) {
        const double Vk = ((k == 0) ? Xd[0] / w0 : Xd[k] / wk) * 2.0;
        const double phase = -piOver2N * static_cast<double>(k);
        const Cd rot(std::cos(phase), std::sin(phase));
        Z[k] = Vk * rot;
    }
    for (size_t k = 1; k < N; ++k) Z[M - k] = std::conj(Z[k]);

    Value Y = ifft(mr, Zv, /*n=*/-1, /*dim=*/0);
    // y is real; first N samples are the recovered signal x.
    const double *yd = (Y.type() == ValueType::COMPLEX)
                       ? nullptr  // need real path below
                       : Y.doubleData();
    if (yd) {
        std::memcpy(xt, yd, N * sizeof(double));
    } else {
        const Cd *Yc = Y.complexData();
        for (size_t n = 0; n < N; ++n) xt[n] = Yc[n].real();
    }
    return r;
}

namespace detail {

void dct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("dct: requires 1 argument",
                     0, 0, "dct", "", "m:dct:nargin");
    outs[0] = dct(ctx.engine->resource(), args[0]);
}

void idct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("idct: requires 1 argument",
                     0, 0, "idct", "", "m:idct:nargin");
    outs[0] = idct(ctx.engine->resource(), args[0]);
}

} // namespace detail

} // namespace numkit::signal
