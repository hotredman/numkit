// toolboxes/stats/src/descriptive/descriptive.cpp
//
// Descriptive statistics: var, std, median, quantile, prctile, mode,
// cov, corrcoef. All take an explicit `dim` argument (1-based, or 0
// for "first non-singleton"). Implementations route through
// applyAlongDim from toolboxes/builtin/src/reduction_helpers.hpp.
//
// Moved from toolboxes/builtin/src/data_analysis/descriptive_statistics/stats.cpp
// in Phase 7b — these are Statistics Toolbox content per MATLAB
// taxonomy, not core MATLAB. Registration as stats.descriptive.* +
// compat.* lives in toolboxes/stats/src/library.cpp.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/stats/distributions/students_t.hpp> // tcdf for corrcoef p-values
#include <numkit/stats/distributions/normal.hpp>     // norminv for corrcoef conf bounds
#include <numkit/stats/nan_aware/nan_aware.hpp>  // var_reg / std_reg / median_reg dispatch into stats:: when 'omitnan' is given

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include "reduction_helpers.hpp"
#include "math/arithmetic/var_reduction.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string>
#include <type_traits>
#include <utility>

#include "descriptive_detail.hpp"

namespace numkit::stats {

// `using namespace` aliases for helpers physically rooted in toolboxes/builtin
// (Phase 7b moved this file out, but the helpers stay there). Reg/impl
// bodies can call applyAlongDim, resolveDim, compactNonNan,
// firstNonSingletonDim, outShapeForDim, outShapeForDimND, sliceLenForDim,
// varianceTwoPass, … by short name as before the move.
using namespace ::numkit::builtin::detail;
using ::numkit::math::varianceTwoPass;

// ────────────────────────────────────────────────────────────────────
// var / std
// ────────────────────────────────────────────────────────────────────
//
// Phase P5 + P2-followup: var / std / nanvar / nanstd all route through
// the SIMD two-pass kernels in backends/MStdVarReduction_{simd,portable}.cpp
// and MStdNanReductions_{simd,portable}.cpp. Welford's recurrence
// (numerically pristine but fully serial) is no longer used here.

Value var(const Value &x, int normFlag, int dim, std::pmr::memory_resource *mr)
{
    validateNormFlag(normFlag, "var");
    if (x.type() == ValueType::COMPLEX)
        return varianceComplex(x, normFlag, dim, mr, /*sqrtIt=*/false);
    if (x.isEmpty())
        return emptyStatReductionNaN(x, dim, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(varianceTwoPass(x.doubleData(), x.numel(), normFlag), mr);

    const int d = resolveDim(x, dim, "var");
    Value r = applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return varianceTwoPass(slice, n, normFlag);
        }, mr);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), mr);
    return r;
}

Value stdev(const Value &x, int normFlag, int dim, std::pmr::memory_resource *mr)
{
    validateNormFlag(normFlag, "std");
    if (x.type() == ValueType::COMPLEX)
        return varianceComplex(x, normFlag, dim, mr, /*sqrtIt=*/true);
    if (x.isEmpty())
        return emptyStatReductionNaN(x, dim, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(std::sqrt(varianceTwoPass(x.doubleData(), x.numel(), normFlag)), mr);

    const int d = resolveDim(x, dim, "std");
    Value r = applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return std::sqrt(varianceTwoPass(slice, n, normFlag));
        }, mr);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), mr);
    return r;
}

// ────────────────────────────────────────────────────────────────────
// median
// ────────────────────────────────────────────────────────────────────
//
// nth_element gives O(n) average instead of O(n log n) full sort.
// The slice is mutated in place — that's fine because the scratch
// buffer is owned by forEachSlice and reused per output index.


Value median(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX)
        return medianComplex(x, dim, mr);   // sort by abs, ties by angle (MATLAB)
    if (x.isEmpty())
        return emptyStatReductionNaN(x, dim, mr);
    const int d = resolveDim(x, dim, "median");
    Value r = applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return medianFromSlice(slice, n);
        }, mr);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), mr);
    else if (isIntegerType(x.type()))
        // MATLAB preserves the integer class: round half-away + saturate.
        r = narrowToInteger(r, x.type(), mr);
    return r;
}

// ────────────────────────────────────────────────────────────────────
// quantile / prctile
// ────────────────────────────────────────────────────────────────────
//
// Three interpolation methods are supported, matching MATLAB R2025b
// (`help quantile` → method ∈ {midpoint, inclusive, exclusive,
// approximate}):
//
//   * Midpoint  (default; R2007a algorithm, Type-5 in Hyndman/Fan)
//                positions: (k-0.5)/N for k = 1..N
//                inverse:   q = p*N + 0.5, clamp [1, N], linear interp
//
//   * Inclusive (Type-7; numkit's old default)
//                positions: (k-1)/(N-1)
//                inverse:   q = p*(N-1) + 1, clamp [1, N], linear interp
//
//   * Exclusive (Type-6; Weibull)
//                positions: k/(N+1)
//                inverse:   q = p*(N+1), clamp [1, N], linear interp
//
//   * Approximate — t-digest, currently falls back to Midpoint
//     (no functional gap, just signature compatibility).

Value quantile(const Value &x, const Value &p, int dim, std::pmr::memory_resource *mr)
{
    return quantileImpl(x, p, dim, 1.0, QMethod::Midpoint, "quantile", mr);
}

Value prctile(const Value &x, const Value &p, int dim, std::pmr::memory_resource *mr)
{
    return quantileImpl(x, p, dim, 0.01, QMethod::Midpoint, "prctile", mr);
}

// Internal entry point used by registration adapters which parsed
// 'all' / vecdim / Method themselves. `flatten = true` collapses x to
// a flat row vector before reduction; `method` selects the algorithm.
Value quantileWithOpts(const Value &x, const Value &p, int dim, bool flatten, QMethod method, double pScale, const char *fn, std::pmr::memory_resource *mr)
{
    if (flatten) {
        // Reshape into a 1×N row, then reduce along dim 2 (the only
        // non-singleton dim — gives the canonical "all" semantics).
        Value flat = Value::matrix(1, x.numel(), ValueType::DOUBLE, mr);
        if (x.numel() > 0) {
            const double *src = x.doubleData();
            std::copy(src, src + x.numel(), flat.doubleDataMut());
        }
        return quantileImpl(flat, p, 2, pScale, method, fn, mr);
    }
    return quantileImpl(x, p, dim, pScale, method, fn, mr);
}

// ────────────────────────────────────────────────────────────────────
// mode
// ────────────────────────────────────────────────────────────────────
//
// MATLAB rule: mode preserves the input element type. The value array
// has the same type as input (DOUBLE/SINGLE/INT*/UINT*/LOGICAL/CHAR);
// the frequency array is always DOUBLE. NaN values are ignored when
// counting (floating types only — integers have no NaN). Ties resolve
// to the smallest value: we sort ascending, then use strict-greater
// comparison so the first run achieving the max count wins.


std::tuple<Value, Value>
mode(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    // MATLAB: mode of an empty array -> NaN-shaped value, 0-shaped count.
    // [M,F]=mode([]) gives M=NaN, F=0; mode(zeros(0,3))=[NaN NaN NaN] with
    // F=[0 0 0]; mode(zeros(3,0))=1x0 empties.
    if (x.isEmpty())
        return { emptyStatReductionFill(x, dim, std::nan(""), mr),
                 emptyStatReductionFill(x, dim, 0.0, mr) };
    const int d = resolveDim(x, dim, "mode");
    return dispatchMode(x, d, mr, "mode");
}

// skewness / kurtosis moved to toolboxes/stats/src/moments/moments.cpp.

// ────────────────────────────────────────────────────────────────────
// cov / corrcoef
// ────────────────────────────────────────────────────────────────────

Value cov(const Value &x, int normFlag, std::pmr::memory_resource *mr)
{
    validateNormFlagCov(normFlag, "cov");
    validateCovInputs(x, "cov");

    ScratchArena scratch(mr);
    ScratchVec<double> data(&scratch);
    std::size_t n, p;
    readMatrix(x, data, n, p);
    if (n == 0) {
        // MATLAB: cov of empty → NaN (or empty p×p depending on shape).
        if (p == 1) return Value::scalar(std::nan(""), mr);
        return Value::matrix(p, p, ValueType::DOUBLE, mr);
    }
    centerColumns(data.data(), n, p);

    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n) - 1.0)
        : static_cast<double>(n);

    if (p == 1) {
        // Vector input → return scalar variance.
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += data[i] * data[i];
        return Value::scalar(s / divisor, mr);
    }
    return covMatrixFromCentered(data.data(), n, p, divisor, mr);
}

Value cov(const Value &x, const Value &y, int normFlag, std::pmr::memory_resource *mr)
{
    validateNormFlagCov(normFlag, "cov");
    validateCovInputs(x, "cov");
    validateCovInputs(y, "cov");
    if (!x.dims().isVector() || !y.dims().isVector())
        throw Error("cov: two-input form requires vector arguments",
                     0, 0, "cov", "", "numkit:cov:notVector");
    if (x.numel() != y.numel())
        throw Error("cov: x and y must have the same length",
                     0, 0, "cov", "", "numkit:cov:lengthMismatch");
    const std::size_t n = x.numel();
    if (n == 0)
        return Value::matrix(2, 2, ValueType::DOUBLE, mr);
    ScratchArena scratch(mr);
    auto data = ScratchVec<double>(n * 2, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        data[i] = x.elemAsDouble(i);          // column 0 (= x)
        data[n + i] = y.elemAsDouble(i);      // column 1 (= y)
    }
    centerColumns(data.data(), n, 2);
    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n) - 1.0)
        : static_cast<double>(n);
    return covMatrixFromCentered(data.data(), n, 2, divisor, mr);
}


Value corrcoef(const Value &x, std::pmr::memory_resource *mr)
{
    // Special case: vector input → 1×1 matrix [1] (variable correlated
    // with itself). Matches MATLAB's `corrcoef(rand(5,1))` behaviour.
    if (x.dims().isVector() || x.isScalar()) {
        auto R = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        R.doubleDataMut()[0] = 1.0;
        return R;
    }
    auto C = cov(x, 0, mr);
    return corrcoefFromCov(C, mr);
}

Value corrcoef(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    auto C = cov(x, y, 0, mr);
    return corrcoefFromCov(C, mr);
}

} // namespace numkit::stats
