#pragma once

#include "MEngine.hpp"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::m {

// ============================================================
// Iterative radix-2 Cooley-Tukey FFT (in-place)
// N must be a power of 2. dir=1 forward, dir=-1 inverse.
// ============================================================
inline void fftRadix2(std::vector<Complex> &buf, int dir)
{
    size_t N = buf.size();
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

    // Butterfly stages
    for (size_t len = 2; len <= N; len <<= 1) {
        double angle = dir * 2.0 * M_PI / static_cast<double>(len);
        Complex wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < N; i += len) {
            Complex w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                Complex u = buf[i + j];
                Complex v = buf[i + j + len / 2] * w;
                buf[i + j]           = u + v;
                buf[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
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
// Prepare complex buffer from MValue (real or complex)
// with zero-padding to fftLen
// ============================================================
inline std::vector<Complex> prepareFFTBuffer(const MValue &x, size_t inputLen, size_t fftLen)
{
    std::vector<Complex> buf(fftLen, Complex(0.0, 0.0));
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
// Pack complex buffer into MValue (1 x outLen)
// ============================================================
inline MValue packComplexResult(const std::vector<Complex> &buf, size_t outLen, Allocator *alloc)
{
    auto r = MValue::complexMatrix(1, outLen, alloc);
    Complex *dst = r.complexDataMut();
    for (size_t i = 0; i < outLen; ++i)
        dst[i] = buf[i];
    return r;
}

// ============================================================
// Direct convolution O(n*m)
// ============================================================
inline std::vector<double> convDirect(const double *a, size_t na,
                                      const double *b, size_t nb)
{
    size_t nc = na + nb - 1;
    std::vector<double> c(nc, 0.0);
    for (size_t i = 0; i < na; ++i)
        for (size_t j = 0; j < nb; ++j)
            c[i + j] += a[i] * b[j];
    return c;
}

// ============================================================
// FFT-based convolution O(N log N)
// ============================================================
inline std::vector<double> convFFT(const double *a, size_t na,
                                   const double *b, size_t nb)
{
    size_t nc = na + nb - 1;
    size_t fftLen = nextPow2(nc);

    std::vector<Complex> fa(fftLen, Complex(0.0, 0.0));
    std::vector<Complex> fb(fftLen, Complex(0.0, 0.0));
    for (size_t i = 0; i < na; ++i) fa[i] = Complex(a[i], 0.0);
    for (size_t i = 0; i < nb; ++i) fb[i] = Complex(b[i], 0.0);

    fftRadix2(fa, 1);
    fftRadix2(fb, 1);

    for (size_t i = 0; i < fftLen; ++i)
        fa[i] *= fb[i];

    fftRadix2(fa, -1);

    double invN = 1.0 / static_cast<double>(fftLen);
    std::vector<double> c(nc);
    for (size_t i = 0; i < nc; ++i)
        c[i] = fa[i].real() * invN;
    return c;
}

// Threshold for switching conv from direct to FFT
constexpr size_t CONV_FFT_THRESHOLD = 500;

} // namespace numkit::m
