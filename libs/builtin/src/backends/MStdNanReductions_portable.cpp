// libs/builtin/src/backends/MStdNanReductions_portable.cpp
//
// Scalar single-pass nansum / nanmean. Compiled when NUMKIT_WITH_SIMD=OFF.
// Same single-read pattern as the Highway variant — branch-free predicate
// (`std::isnan` + ternary) keeps it autovectorisable by recent compilers
// even on the portable build.

#include "MStdNanReductions.hpp"

#include <cmath>
#include <cstddef>

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

} // namespace numkit::m::builtin
