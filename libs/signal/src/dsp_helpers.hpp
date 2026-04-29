#pragma once

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <memory_resource>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit {

// ============================================================
// Twiddle-factor precomputation
//
// W[k] = exp(dir * 2πi * k / N) for k in [0, N/2). Stage `len` of an
// N-point radix-2 FFT uses W[0], W[step], W[2·step], …,
// W[(len/2 - 1)·step] where step = N/len. A single table of length
// N/2 thus covers every stage — removing the rotating `w *= wlen`
// from the inner loop and breaking the data dependency that prevents
// SIMD vectorisation (relevant for Phase 8e.2+).
//
// dir=+1 matches the current sign convention used by fftRadix2 and
// its conjugate-trick inverse wrapper in fft.cpp.
// ============================================================
inline void fillFftTwiddles(Complex *W, size_t N, int dir)
{
    const size_t half = N / 2;
    if (half == 0) return;
    const double base = dir * 2.0 * M_PI / static_cast<double>(N);
    for (size_t k = 0; k < half; ++k) {
        const double a = base * static_cast<double>(k);
        W[k] = Complex(std::cos(a), std::sin(a));
    }
}

// ============================================================
// Iterative radix-2 Cooley-Tukey FFT (in-place).
// N must be a power of 2. Takes a precomputed twiddle table W of
// length N/2; see fillFftTwiddles above for how to build it.
//
// Decoupled from any container type so callers can back the buffer
// with std::vector, std::pmr::vector, a stack array, or any other
// contiguous Complex storage.
// ============================================================
inline void fftRadix2(Complex *buf, size_t N, const Complex *W)
{
    if (N <= 1)
        return;

    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < N; ++i) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(buf[i], buf[j]);
    }

    // Butterfly stages — look up twiddles from the precomputed table.
    for (size_t len = 2; len <= N; len <<= 1) {
        const size_t step = N / len;
        for (size_t i = 0; i < N; i += len) {
            for (size_t j = 0; j < len / 2; ++j) {
                const Complex w = W[j * step];
                const Complex u = buf[i + j];
                const Complex v = buf[i + j + len / 2] * w;
                buf[i + j]           = u + v;
                buf[i + j + len / 2] = u - v;
            }
        }
    }
}

// Convenience overload: builds a one-shot twiddle table from the
// caller-provided arena (`mr`) on each call. Still used by callers that
// haven't hoisted the table outside their own loops (spectral_analysis/,
// time_frequency/, transforms/hilbert.cpp). For hot paths
// (fftAlongDim in fft.cpp), prefer the primary overload + fillFftTwiddles
// so the table cost amortises.
inline void fftRadix2(std::pmr::memory_resource *mr,
                      Complex *buf, size_t N, int dir)
{
    if (N <= 1) return;
    ScratchVec<Complex> W(N / 2, mr);
    fillFftTwiddles(W.data(), N, dir);
    fftRadix2(buf, N, W.data());
}

// Container overload — buf.data() + buf.size() + dir. Handles both
// std::vector<Complex> and std::pmr::vector<Complex>.
template <typename Container>
inline auto fftRadix2(std::pmr::memory_resource *mr, Container &buf, int dir)
    -> decltype(buf.data(), buf.size(), void())
{
    fftRadix2(mr, buf.data(), buf.size(), dir);
}

// ============================================================
// Next power of 2 >= n
// ============================================================
inline size_t nextPow2(size_t n)
{
    if (n == 0)
        return 1;
    size_t p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

// ============================================================
// Prepare complex buffer from Value (real or complex)
// with zero-padding to fftLen.
// ============================================================
inline ScratchVec<Complex> prepareFFTBuffer(std::pmr::memory_resource *mr,
                                            const Value &x,
                                            size_t inputLen, size_t fftLen)
{
    ScratchVec<Complex> buf(fftLen, mr);
    if (x.isComplex()) {
        const Complex *src = x.complexData();
        for (size_t i = 0; i < inputLen; ++i)
            buf[i] = src[i];
    } else {
        const double *src = x.doubleData();
        for (size_t i = 0; i < inputLen; ++i)
            buf[i] = Complex(src[i], 0.0);
    }
    return buf;
}

// ============================================================
// Pack complex buffer into Value (1 x outLen).
// Pointer + size, not a container reference, so the same helper
// composes with std::vector, std::pmr::vector, raw arrays, etc.
// ============================================================
inline Value packComplexResult(const Complex *buf, size_t outLen, std::pmr::memory_resource *mr)
{
    auto r = Value::complexMatrix(1, outLen, mr);
    Complex *dst = r.complexDataMut();
    for (size_t i = 0; i < outLen; ++i)
        dst[i] = buf[i];
    return r;
}

// ============================================================
// Direct convolution O(n*m)
// ============================================================
inline ScratchVec<double> convDirect(std::pmr::memory_resource *mr,
                                     const double *a, size_t na,
                                     const double *b, size_t nb)
{
    size_t nc = na + nb - 1;
    ScratchVec<double> c(nc, mr);
    for (size_t i = 0; i < na; ++i)
        for (size_t j = 0; j < nb; ++j)
            c[i + j] += a[i] * b[j];
    return c;
}

// ============================================================
// FFT-based convolution O(N log N)
// ============================================================
inline ScratchVec<double> convFFT(std::pmr::memory_resource *mr,
                                  const double *a, size_t na,
                                  const double *b, size_t nb)
{
    size_t nc = na + nb - 1;
    size_t fftLen = nextPow2(nc);

    ScratchVec<Complex> fa(fftLen, mr);
    ScratchVec<Complex> fb(fftLen, mr);
    for (size_t i = 0; i < na; ++i) fa[i] = Complex(a[i], 0.0);
    for (size_t i = 0; i < nb; ++i) fb[i] = Complex(b[i], 0.0);

    // Forward twiddles used for both fa and fb; inverse twiddles used
    // once for fa. Hoisting them here saves two tables' worth of work
    // vs the convenience overload.
    ScratchVec<Complex> W_fwd(fftLen / 2, mr);
    ScratchVec<Complex> W_inv(fftLen / 2, mr);
    fillFftTwiddles(W_fwd.data(), fftLen, +1);
    fillFftTwiddles(W_inv.data(), fftLen, -1);

    fftRadix2(fa.data(), fftLen, W_fwd.data());
    fftRadix2(fb.data(), fftLen, W_fwd.data());

    for (size_t i = 0; i < fftLen; ++i)
        fa[i] *= fb[i];

    fftRadix2(fa.data(), fftLen, W_inv.data());

    double invN = 1.0 / static_cast<double>(fftLen);
    ScratchVec<double> c(nc, mr);
    for (size_t i = 0; i < nc; ++i)
        c[i] = fa[i].real() * invN;
    return c;
}

// Threshold for switching conv from direct to FFT
constexpr size_t CONV_FFT_THRESHOLD = 500;

} // namespace numkit
