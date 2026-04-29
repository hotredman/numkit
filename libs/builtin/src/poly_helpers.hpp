// libs/builtin/src/poly_helpers.hpp
//
// Polynomial helpers shared between the public roots() builtin and the
// DSP-side filter design code (tf2sos / tf2zp / butter). All-inline; no
// matching .cpp.

#pragma once

#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/types.hpp>  // Complex

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <memory_resource>

namespace numkit::builtin::detail {

using Complex = std::complex<double>;

// Horner evaluation of a polynomial p(z) and its derivative p'(z). The
// coefficients are in MATLAB / numpy order: c[0] is the leading
// coefficient (highest power), c[n] is the constant. Pointer + size
// instead of a container ref so it composes with std::vector,
// std::pmr::vector, or raw arrays.
inline void polyEvalHorner(const Complex *c, std::size_t n,
                           Complex z, Complex &outP, Complex &outDP)
{
    Complex p  = c[0];
    Complex dp(0.0, 0.0);
    for (std::size_t i = 1; i < n; ++i) {
        dp = dp * z + p;
        p  = p  * z + c[i];
    }
    outP  = p;
    outDP = dp;
}

// Durand-Kerner simultaneous root finder for a real-coefficient
// polynomial. Coefficients in MATLAB order. Returns a vector of length
// equal to the polynomial degree; complex-conjugate pairs are not
// guaranteed to come out exactly conjugate (they're each found
// independently), but typically agree to ~1e-13.
//
// Behaviour at edge cases:
//   * Leading zeros are stripped (degree drop).
//   * Trailing zeros become explicit roots at the origin.
//   * Constant or empty input → no roots.
//
// ~50-200 iterations to ~1e-14 tolerance for moderate polynomial
// degrees (≤ 32). Higher degrees may need a tighter inner loop, but
// practical SOS / roots() usage rarely exceeds this.
inline ScratchVec<Complex>
polyRootsDurandKerner(std::pmr::memory_resource *mr,
                      const double *coeffs, std::size_t coeffsN)
{
    std::size_t lo = 0, hi = coeffsN;
    while (lo < hi && coeffs[lo] == 0.0) ++lo;
    std::size_t origZeros = 0;
    while (hi > lo && coeffs[hi - 1] == 0.0) { --hi; ++origZeros; }
    if (hi - lo <= 1)
        return ScratchVec<Complex>(origZeros, Complex(0.0, 0.0), mr);

    ScratchVec<Complex> c(hi - lo, mr);
    for (std::size_t i = 0; i < c.size(); ++i)
        c[i] = Complex(coeffs[lo + i] / coeffs[lo], 0.0);
    const std::size_t n = c.size() - 1;

    // Initial guesses on the Cauchy-bound circle, offset to avoid
    // a real-axis cluster (factor of e^{j·0.4} chosen empirically).
    double maxCoef = 0.0;
    for (std::size_t i = 1; i <= n; ++i)
        maxCoef = std::max(maxCoef, std::abs(c[i].real()));
    const double R = 1.0 + maxCoef;
    ScratchVec<Complex> roots(n, mr);
    constexpr double kTwoPi = 6.28318530717958647692;
    for (std::size_t k = 0; k < n; ++k) {
        const double theta = kTwoPi * static_cast<double>(k)
                             / static_cast<double>(n) + 0.4;
        roots[k] = Complex(R * std::cos(theta), R * std::sin(theta));
    }

    constexpr int    kMaxIter = 200;
    constexpr double kTol     = 1e-14;
    for (int it = 0; it < kMaxIter; ++it) {
        double maxStep = 0.0;
        for (std::size_t k = 0; k < n; ++k) {
            Complex p, dp;
            polyEvalHorner(c.data(), c.size(), roots[k], p, dp);
            Complex denom(1.0, 0.0);
            for (std::size_t j = 0; j < n; ++j)
                if (j != k) denom *= (roots[k] - roots[j]);
            if (std::abs(denom) < 1e-300) continue;
            const Complex step = p / denom;
            roots[k] -= step;
            const double s = std::abs(step);
            if (s > maxStep) maxStep = s;
        }
        if (maxStep < kTol) break;
    }
    for (std::size_t i = 0; i < origZeros; ++i)
        roots.push_back(Complex(0.0, 0.0));
    return roots;
}

// Expand a list of polynomial roots into a real-coefficient polynomial
// via repeated convolution by (x - r). Output length = rootsN + 1
// in MATLAB order (leading coefficient first). Imaginary residue is
// dropped — caller is responsible for ensuring the roots come in
// complex-conjugate pairs (otherwise the imaginary parts won't cancel
// to zero).
inline ScratchVec<double>
polyExpandFromRoots(std::pmr::memory_resource *mr,
                    const Complex *roots, std::size_t rootsN)
{
    const int n = static_cast<int>(rootsN);
    ScratchVec<Complex> poly(static_cast<std::size_t>(n + 1), mr);
    poly[0] = Complex(1.0, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j >= 1; --j)
            poly[j] = poly[j] - roots[i] * poly[j - 1];
    ScratchVec<double> r(static_cast<std::size_t>(n + 1), mr);
    for (int i = 0; i <= n; ++i)
        r[i] = poly[i].real();
    return r;
}

} // namespace numkit::builtin::detail
