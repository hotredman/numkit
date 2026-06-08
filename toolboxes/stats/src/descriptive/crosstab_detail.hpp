// toolboxes/.../crosstab_detail.hpp — private compute substrate (anon-in-header,
// internal linkage per TU) shared by crosstab.cpp compute and crosstab_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

// Sorted-unique values from x (NaN excluded).
std::vector<double> sortedUniqueNoNaN(const Value &x)
{
    const size_t N = x.numel();
    std::vector<double> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (std::isnan(xi)) continue;
        v.push_back(xi);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

// Find index of value in sorted unique list (binary search).
size_t indexOf(const std::vector<double> &u, double val)
{
    auto it = std::lower_bound(u.begin(), u.end(), val);
    return static_cast<size_t>(it - u.begin());
}

// num2str-style label for a (typically integer) group value, matching
// MATLAB crosstab's `labels` output. Integer values print without a
// decimal point; non-integers fall back to %.5g (num2str's default
// significant-digit count for the simple decimals seen in group ids).
std::string labelString(double v)
{
    char buf[64];
    if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e15)
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    else
        std::snprintf(buf, sizeof(buf), "%.5g", v);
    return std::string(buf);
}

} // namespace

} // namespace numkit::stats
