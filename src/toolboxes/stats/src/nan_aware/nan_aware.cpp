// toolboxes/stats/src/nan_aware/nan_aware.cpp
//
// nansum / nanmean / nanmax / nanmin / nanvar / nanstdev / nanmedian.
// Extracted from toolboxes/builtin/src/stats.cpp during the
// MATLAB-taxonomy refactor — Statistics Toolbox content.

#include <numkit/stats/nan_aware/nan_aware.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include <numkit/ops/reductions.hpp>
#include "backends/nan_reductions.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "nan_aware_detail.hpp"

namespace numkit::stats {

using ::numkit::ops::applyAlongDim;
using ::numkit::ops::resolveDim;
using ::numkit::stats::detail::nanSumScan;
using ::numkit::stats::detail::nanSumCountScan;
using ::numkit::stats::detail::nanMaxScan;
using ::numkit::stats::detail::nanMinScan;
using ::numkit::stats::detail::nanVarianceTwoPass;
using ::numkit::ops::compactNonNan;


Value nansum(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanSumScan(x.doubleData(), x.numel()), mr);

    const int d = resolveDim(x, dim, "nansum");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanSumScan(slice, n); // all-NaN → 0
        }, mr);
}

Value nanmean(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE) {
        const auto r = nanSumCountScan(x.doubleData(), x.numel());
        return Value::scalar(r.count > 0 ? r.sum / static_cast<double>(r.count)
                                          : std::nan(""), mr);
    }

    const int d = resolveDim(x, dim, "nanmean");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            const auto r = nanSumCountScan(slice, n);
            return r.count > 0 ? r.sum / static_cast<double>(r.count)
                               : std::nan("");
        }, mr);
}

Value nanmax(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanMaxScan(x.doubleData(), x.numel()), mr);

    const int d = resolveDim(x, dim, "nanmax");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanMaxScan(slice, n);
        }, mr);
}

Value nanmin(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanMinScan(x.doubleData(), x.numel()), mr);

    const int d = resolveDim(x, dim, "nanmin");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanMinScan(slice, n);
        }, mr);
}

Value nanvar(const Value &x, int normFlag, int dim, std::pmr::memory_resource *mr)
{
    validateNormFlag(normFlag, "nanvar");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanVarianceTwoPass(x.doubleData(), x.numel(), normFlag), mr);

    const int d = resolveDim(x, dim, "nanvar");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return nanVarianceTwoPass(slice, n, normFlag);
        }, mr);
}

Value nanstdev(const Value &x, int normFlag, int dim, std::pmr::memory_resource *mr)
{
    validateNormFlag(normFlag, "nanstd");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(std::sqrt(nanVarianceTwoPass(x.doubleData(), x.numel(), normFlag)), mr);

    const int d = resolveDim(x, dim, "nanstd");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return std::sqrt(nanVarianceTwoPass(slice, n, normFlag));
        }, mr);
}

Value nanmedian(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "nanmedian");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            const size_t k = compactNonNan(slice, n);
            return medianFromSlice(slice, k); // returns NaN at k==0
        }, mr);
}

// ── nancov ─────────────────────────────────────────────────────────
//
// `complete` mode only (== MATLAB default `cov(X, 'omitrows')`): drop
// every observation row that has at least one NaN, then standard
// covariance. `'pairwise'` mode is a documented v1 gap.


Value nancov(const Value &x, int normFlag, std::pmr::memory_resource *mr)
{
    validateNormFlag(normFlag, "nancov");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> raw(&scratch);
    std::size_t n_orig = 0, p = 0;
    readObsMatrix(x, raw, n_orig, p);

    // Drop NaN-containing rows. We can do this in-place on `raw` since
    // dropNanRows compacts within each column down to row index w-1.
    const std::size_t n_kept = dropNanRows(raw.data(), n_orig, p);

    if (n_kept == 0) {
        // All rows dropped → MATLAB returns NaN-filled p×p (or scalar NaN for vector).
        if (p == 1) return Value::scalar(std::nan(""), mr);
        auto out = Value::matrix(p, p, ValueType::DOUBLE, mr);
        double *d = out.doubleDataMut();
        for (std::size_t k = 0; k < p * p; ++k) d[k] = std::nan("");
        return out;
    }

    // Pack the kept rows into a tight n_kept × p layout.
    ScratchVec<double> tight(n_kept * p, &scratch);
    packKeptRows(raw.data(), n_orig, n_kept, p, tight.data());

    centerColumnsLocal(tight.data(), n_kept, p);
    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n_kept) - 1.0)
        : static_cast<double>(n_kept);

    if (p == 1) {
        double s = 0.0;
        for (std::size_t i = 0; i < n_kept; ++i)
            s += tight[i] * tight[i];
        return Value::scalar(s / divisor, mr);
    }
    return covFromCenteredLocal(tight.data(), n_kept, p, divisor, mr);
}

Value nancov(const Value &x, const Value &y, int normFlag,
             std::pmr::memory_resource *mr)
{
    validateNormFlag(normFlag, "nancov");
    if (!x.dims().isVector() || !y.dims().isVector())
        throw Error("nancov: two-input form requires vector arguments",
                    0, 0, "nancov", "", "numkit:nancov:notVector");
    if (x.numel() != y.numel())
        throw Error("nancov: x and y must have the same length",
                    0, 0, "nancov", "", "numkit:nancov:lengthMismatch");

    const std::size_t n = x.numel();
    if (n == 0)
        return Value::matrix(2, 2, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    auto data = ScratchVec<double>(n * 2, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        data[i]       = x.elemAsDouble(i);      // column 0
        data[n + i]   = y.elemAsDouble(i);      // column 1
    }
    const std::size_t n_kept = dropNanRows(data.data(), n, 2);
    if (n_kept == 0) {
        auto out = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        double *d = out.doubleDataMut();
        for (std::size_t k = 0; k < 4; ++k) d[k] = std::nan("");
        return out;
    }
    ScratchVec<double> tight(n_kept * 2, &scratch);
    packKeptRows(data.data(), n, n_kept, 2, tight.data());
    centerColumnsLocal(tight.data(), n_kept, 2);
    const double divisor = (normFlag == 0)
        ? std::max(1.0, static_cast<double>(n_kept) - 1.0)
        : static_cast<double>(n_kept);
    return covFromCenteredLocal(tight.data(), n_kept, 2, divisor, mr);
}

} // namespace numkit::stats
