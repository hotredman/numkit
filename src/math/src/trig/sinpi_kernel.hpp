// toolboxes/builtin/src/math/trig/sinpi_kernel.hpp
//
// Accurate sinpi / cospi scalar kernel, shared by the SIMD backend
// (trig_highway.cpp, used for the loop tail) and the portable backend
// (trig_portable.cpp). Computes sin(pi*x) / cos(pi*x) with an exact
// octant reduction in integer arithmetic followed by a single-double
// minimax polynomial, so integer arguments give an exact 0 and the
// value at e.g. 1/6 is 0.5 to full precision -- matching MATLAB R2025b,
// unlike the naive sin(pi*x) which loses ~1e-10 by x=1e7.
//
// Polynomial coefficients (and the d*4 octant reduction) are ported
// from SLEEF's sinpik/cospik kernels (Boost Software License 1.0; see
// third_party/sleef/). Only the leading double of each split constant
// is used (single-double evaluation): this is <=2 ULP across the range,
// which is well inside MATLAB-parity tolerance and avoids the cost of
// double-double arithmetic. The 32-bit-lane range guard of the SLEEF
// original is replaced by a 64-bit octant index so large arguments
// (e.g. sinpi(1e10+0.5) == 1) stay correct instead of flushing to 0.

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace numkit::math::detail {

// sinpik / cospik share one coefficient table; only the o-selection and
// the final sign differ. Index 0 is the leading (highest-degree) term.
inline constexpr double kSinpiPoly_o[8] = {
     9.94480387626843774090208e-16,
    -3.89796226062932799164047e-13,
     1.150115825399960352669010e-10,
    -2.46113695010446974953590e-08,
     3.590860448590527540050620e-06,
    -0.000325991886927389905997954,
     0.0158543442438155018914259,
    -0.308425137534042437259529,
};
inline constexpr double kSinpiPoly_e[8] = {
    -2.02461120785182399295868e-14,
     6.948218305801794613277840e-12,
    -1.75724749952853179952664e-09,
     3.133616889668683928784220e-07,
    -3.65762041821615519203610e-05,
     0.0024903945701927185027435600,
    -0.0807455121882807852484731,
     0.785398163397448278999491,
};

// Beyond 2^53 a double is an even integer: sinpi == 0, cospi == 1.
inline constexpr double kSinpiIntThreshold = 9007199254740992.0; // 2^53

inline double sinpi_poly(bool o, double s)
{
    const double *c = o ? kSinpiPoly_o : kSinpiPoly_e;
    double x = c[0];
    for (int k = 1; k < 8; ++k)
        x = x * s + c[k];
    return x;
}

// Nearest even integer to u, via SLEEF's (trunc(u) + (u>=0)) & ~1.
inline std::int64_t sinpi_octant(double u)
{
    std::int64_t q = static_cast<std::int64_t>(u); // truncate toward zero
    q += (q < 0) ? 0 : 1;
    return q & ~static_cast<std::int64_t>(1);
}

inline double sinpi_kernel(double d)
{
    if (std::isnan(d)) return d;
    if (std::isinf(d)) return std::numeric_limits<double>::quiet_NaN();
    if (std::fabs(d) >= kSinpiIntThreshold) return std::copysign(0.0, d);

    const double        u = d * 4.0;
    const std::int64_t  q = sinpi_octant(u);
    const bool          o = (q & 2) != 0;
    const double        t = u - static_cast<double>(q);
    const double        s = t * t;

    double x = sinpi_poly(o, s) * (o ? s : t);
    if (o) x += 1.0;
    x = (q & 4) ? -x : x;
    // sinpi(integer) is an exact zero whose sign MATLAB takes from the
    // input (sinpi(1)==+0, sinpi(-1)==-0), not from the quadrant.
    return (x == 0.0) ? std::copysign(0.0, d) : x;
}

inline double cospi_kernel(double d)
{
    if (std::isnan(d)) return d;
    if (std::isinf(d)) return std::numeric_limits<double>::quiet_NaN();
    if (std::fabs(d) >= kSinpiIntThreshold) return 1.0;

    const double        u = d * 4.0;
    const std::int64_t  q = sinpi_octant(u);
    const bool          o = (q & 2) == 0;
    const double        t = u - static_cast<double>(q);
    const double        s = t * t;

    double x = sinpi_poly(o, s) * (o ? s : t);
    if (o) x += 1.0;
    x = ((q + 2) & 4) ? -x : x;
    // cospi(half-integer) is +0 in MATLAB regardless of side.
    return (x == 0.0) ? 0.0 : x;
}

} // namespace numkit::math::detail
