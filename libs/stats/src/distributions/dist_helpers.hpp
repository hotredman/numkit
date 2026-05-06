// libs/stats/src/distributions/dist_helpers.hpp
//
// Shared helpers for the distribution-CDF adapters. Currently exposes
// the `'upper'` flag detection that all MATLAB *cdf functions accept:
//
//   p = <dist>cdf(x, ..., 'upper')   →   1 - F(x; ...)
//
// Used by every cdf_reg adapter under libs/stats/src/distributions/.

#pragma once

#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace numkit::stats::detail {

// If the last positional arg is a string equal to 'upper' (any case),
// strip it and set `upper = true`. Returns the effective trailing-arg
// count. Adapters should consume args[0..count-1] for positional
// parsing and ignore args[count..] (which is at most the 'upper' flag).
inline size_t stripUpperFlag(Span<const Value> args, bool &upper)
{
    upper = false;
    if (args.empty()) return 0;
    const Value &last = args[args.size() - 1];
    if (!last.isChar() && !last.isString()) return args.size();
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "upper") {
        upper = true;
        return args.size() - 1;
    }
    return args.size();
}

// Apply the upper-tail transform in place: y = 1 - y for every finite
// element. NaN passes through unchanged.
inline void applyUpperInPlace(Value &y)
{
    if (y.type() != ValueType::DOUBLE) return;
    double *d = y.doubleDataMut();
    const size_t n = y.numel();
    for (size_t i = 0; i < n; ++i) {
        const double v = d[i];
        if (!std::isnan(v)) d[i] = 1.0 - v;
    }
}

} // namespace numkit::stats::detail
