// libs/builtin/src/backends/MStdVarReduction_portable.cpp
//
// Scalar two-pass variance. Compiled when NUMKIT_WITH_SIMD=OFF. Same
// numerical recipe as the Highway variant — a tight scalar loop is
// reliably autovectorised by recent compilers and the FMA in the
// inner-loop accumulator gives the same single-rounding behaviour.

#include "MStdVarReduction.hpp"

#include <cmath>
#include <cstddef>

namespace numkit::m::builtin {

double sumScan(const double *p, std::size_t n)
{
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) s += p[i];
    return s;
}

double sumSquaredDeviationsScan(const double *p, std::size_t n, double mean)
{
    double ss = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = p[i] - mean;
        ss += d * d;
    }
    return ss;
}

double varianceTwoPass(const double *p, std::size_t n, int normFlag)
{
    if (n == 0) return std::nan("");
    if (n == 1) return (normFlag == 1) ? 0.0 : std::nan("");
    const double mean  = sumScan(p, n) / static_cast<double>(n);
    const double ss    = sumSquaredDeviationsScan(p, n, mean);
    const double denom = (normFlag == 1) ? static_cast<double>(n)
                                         : static_cast<double>(n - 1);
    return ss / denom;
}

} // namespace numkit::m::builtin
