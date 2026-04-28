// libs/builtin/src/MStdPolyHelpers.hpp
//
// Polynomial helpers shared between the public roots() builtin and the
// DSP-side filter design code (tf2sos / future tf2zp). All-inline; no
// matching .cpp.

#pragma once

#include <numkit/m/core/MTypes.hpp>  // Complex

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace numkit::m::builtin::detail {

using Complex = std::complex<double>;

// Horner evaluation of a polynomial p(z) and its derivative p'(z). The
// coefficients vector is in MATLAB / numpy order: c[0] is the leading
// coefficient (highest power), c[n] is the constant.
inline void polyEvalHorner(const std::vector<Complex> &c, Complex z,
                           Complex &outP, Complex &outDP)
{
    Complex p  = c[0];
    Complex dp(0.0, 0.0);
    for (std::size_t i = 1; i < c.size(); ++i) {
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
inline std::vector<Complex>
polyRootsDurandKerner(const std::vector<double> &coeffs)
{
    std::size_t lo = 0, hi = coeffs.size();
    while (lo < hi && coeffs[lo] == 0.0) ++lo;
    std::size_t origZeros = 0;
    while (hi > lo && coeffs[hi - 1] == 0.0) { --hi; ++origZeros; }
    if (hi - lo <= 1)
        return std::vector<Complex>(origZeros, Complex(0.0, 0.0));

    std::vector<Complex> c(hi - lo);
    for (std::size_t i = 0; i < c.size(); ++i)
        c[i] = Complex(coeffs[lo + i] / coeffs[lo], 0.0);
    const std::size_t n = c.size() - 1;

    // Initial guesses on the Cauchy-bound circle, offset to avoid
    // a real-axis cluster (factor of e^{j·0.4} chosen empirically).
    double maxCoef = 0.0;
    for (std::size_t i = 1; i <= n; ++i)
        maxCoef = std::max(maxCoef, std::abs(c[i].real()));
    const double R = 1.0 + maxCoef;
    std::vector<Complex> roots(n);
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
            polyEvalHorner(c, roots[k], p, dp);
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

} // namespace numkit::m::builtin::detail
