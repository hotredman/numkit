// ops/src/quantile_select.cpp
//
// nth_element-based linear-interpolation quantile, moved verbatim from stats'
// descriptive_extras_detail.hpp (sliceQuantile) so the select primitive lives
// in the kernel layer. See quantile_select.hpp.

#include <numkit/ops/quantile_select.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace numkit::ops {

double sliceQuantile(double *s, std::size_t n, double p)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 1) return s[0];
    // The MATLAB "+0.5" rule needs at most two adjacent order statistics, so
    // partition with nth_element (O(n) average) instead of a full O(n log n)
    // sort. The selected values are exactly the sorted ones, so results are
    // bit-for-bit unchanged — only the cost drops (a two-call iqr over ~10^7
    // elements goes from ~1.7 s of sorting to well under 0.1 s). Shared by
    // iqr / mad / median-of-slice.
    const double q = p * static_cast<double>(n) + 0.5;
    if (q <= 1.0) return *std::min_element(s, s + n);
    if (q >= static_cast<double>(n)) return *std::max_element(s, s + n);
    const std::size_t lo   = static_cast<std::size_t>(std::floor(q)) - 1;
    const double      frac = q - std::floor(q);
    // Place the lo-th smallest at s[lo]; everything in (lo, n) is then >= it,
    // so the (lo+1)-th smallest is the minimum of that right partition.
    std::nth_element(s, s + lo, s + n);
    const double a = s[lo];
    const double b = *std::min_element(s + lo + 1, s + n);
    return a + frac * (b - a);
}

} // namespace numkit::ops
