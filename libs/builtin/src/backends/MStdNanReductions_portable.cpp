// libs/builtin/src/backends/MStdNanReductions_portable.cpp
//
// Scalar single-pass nansum / nanmean. Compiled when NUMKIT_WITH_SIMD=OFF.
// Same single-read pattern as the Highway variant — branch-free predicate
// (`std::isnan` + ternary) keeps it autovectorisable by recent compilers
// even on the portable build.

#include "MStdNanReductions.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

namespace numkit::m::builtin {

double nanSumScan(const double *p, std::size_t n)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isnan(p[i])) sum += p[i];
    return sum;
}

NanSumCount nanSumCountScan(const double *p, std::size_t n)
{
    double sum = 0.0;
    std::size_t cnt = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(p[i])) {
            sum += p[i];
            ++cnt;
        }
    }
    return {sum, cnt};
}

double nanMaxScan(const double *p, std::size_t n)
{
    double best = -std::numeric_limits<double>::infinity();
    bool seen = false;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(p[i])) {
            if (!seen || p[i] > best) best = p[i];
            seen = true;
        }
    }
    return seen ? best : std::nan("");
}

double nanMinScan(const double *p, std::size_t n)
{
    double best = std::numeric_limits<double>::infinity();
    bool seen = false;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(p[i])) {
            if (!seen || p[i] < best) best = p[i];
            seen = true;
        }
    }
    return seen ? best : std::nan("");
}

double nanVarianceTwoPass(const double *p, std::size_t n, int normFlag)
{
    if (n == 0) return std::nan("");
    const auto sc = nanSumCountScan(p, n);
    if (sc.count == 0) return std::nan("");
    if (sc.count == 1) return (normFlag == 1) ? 0.0 : std::nan("");
    const double mean = sc.sum / static_cast<double>(sc.count);
    double ss = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(p[i])) {
            const double d = p[i] - mean;
            ss += d * d;
        }
    }
    const double denom = (normFlag == 1) ? static_cast<double>(sc.count)
                                         : static_cast<double>(sc.count - 1);
    return ss / denom;
}

} // namespace numkit::m::builtin
