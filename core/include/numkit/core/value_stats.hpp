#pragma once
//
// value_stats.hpp — display statistics over a numeric Value.
//
// Single source of truth for the optional Min/Max/Range/Mean/Median/Mode/
// Var/Std columns shown in the IDE's Variable / struct viewer. Used by both
// the core workspace serializer (Engine::workspaceJSON) and the WASM
// inspect-cell serializer (repl_bindings emitInspectCell), so the numbers
// match wherever a value is displayed.
//
// Non-finite elements are skipped (omitnan-style) so a stray NaN doesn't
// blank the whole row. complex → magnitude, logical → 0/1. Variance is the
// sample (N-1) form; mode is the smallest most-frequent value (MATLAB).

#include <numkit/core/value.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

namespace numkit {

struct ValueStats {
    double min, max, mean, median, mode, var, std;
};

// Stats over the half-open element range [start, start+count) in
// column-major linear order. `count == SIZE_MAX` means "to the end".
// The range form drives per-page stats for 3-D / N-D arrays: page p is the
// contiguous block [p*rows*cols, (p+1)*rows*cols). Returns false for
// non-numeric types or when no finite element remains in the range.
inline bool computeValueStatsRange(const Value &val, std::size_t start,
                                   std::size_t count, ValueStats &s)
{
    const std::size_t numel = val.numel();
    if (start > numel) start = numel;
    const std::size_t end =
        (count > numel - start) ? numel : start + count;  // clamp to numel
    std::vector<double> v;
    v.reserve(end - start);
    if (val.type() == ValueType::DOUBLE) {
        const double *p = val.doubleData();
        for (std::size_t i = start; i < end; ++i)
            if (std::isfinite(p[i])) v.push_back(p[i]);
    } else if (val.type() == ValueType::LOGICAL) {
        const std::uint8_t *p = val.logicalData();
        for (std::size_t i = start; i < end; ++i) v.push_back(p[i] ? 1.0 : 0.0);
    } else if (val.type() == ValueType::COMPLEX) {
        const Complex *p = val.complexData();
        for (std::size_t i = start; i < end; ++i) {
            double m = std::hypot(p[i].real(), p[i].imag());
            if (std::isfinite(m)) v.push_back(m);
        }
    } else if (isFloatType(val.type()) || isIntegerType(val.type())) {
        // SINGLE + INT8..UINT64 — read each element as double. (DOUBLE is
        // handled by the fast path above; integers are always finite, so
        // the isfinite filter only ever skips a single's NaN/Inf.)
        for (std::size_t i = start; i < end; ++i) {
            double x = val.elemAsDouble(i);
            if (std::isfinite(x)) v.push_back(x);
        }
    } else {
        return false;
    }
    const std::size_t n = v.size();
    if (n == 0) return false;

    double sum = 0.0, mn = v[0], mx = v[0];
    for (double x : v) { sum += x; if (x < mn) mn = x; if (x > mx) mx = x; }
    s.min = mn; s.max = mx; s.mean = sum / static_cast<double>(n);

    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    s.median = (n % 2) ? sorted[n / 2]
                       : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);

    double acc = 0.0;
    for (double x : v) { double d = x - s.mean; acc += d * d; }
    s.var = (n >= 2) ? acc / static_cast<double>(n - 1) : 0.0;
    s.std = std::sqrt(s.var);

    // Mode: longest run in the sorted data; ties resolve to the smaller
    // value (ascending sort → keep the first run that reaches the max).
    double bestVal = sorted[0];
    std::size_t bestCnt = 1, curCnt = 1;
    for (std::size_t i = 1; i < n; ++i) {
        curCnt = (sorted[i] == sorted[i - 1]) ? curCnt + 1 : 1;
        if (curCnt > bestCnt) { bestCnt = curCnt; bestVal = sorted[i]; }
    }
    s.mode = bestVal;
    return true;
}

// Whole-array convenience overload — stats over every element.
inline bool computeValueStats(const Value &val, ValueStats &s)
{
    return computeValueStatsRange(val, 0, val.numel(), s);
}

}  // namespace numkit
