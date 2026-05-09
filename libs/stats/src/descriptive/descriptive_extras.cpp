// libs/stats/src/descriptive/descriptive_extras.cpp
//
// Descriptive stats extras (B2): bounds, iqr, maxk, mink, rmse.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

using ::numkit::builtin::detail::applyAlongDim;
using ::numkit::builtin::detail::firstNonSingletonDim;
using ::numkit::builtin::detail::validateDim;
using ::numkit::builtin::detail::applyAlongDimWithIndex;

int resolveDim(const Value &x, int dim, const char *fn)
{
    if (dim == 0) return firstNonSingletonDim(x);
    return validateDim(x, dim, fn);
}

double sliceMin(const double *s, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = s[0];
    for (size_t i = 1; i < n; ++i)
        if (s[i] < m || std::isnan(m)) m = s[i];
    return m;
}

double sliceMax(const double *s, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = s[0];
    for (size_t i = 1; i < n; ++i)
        if (s[i] > m || std::isnan(m)) m = s[i];
    return m;
}

// Linear-interpolation quantile of a slice (MATLAB default, type 7).
// MATLAB R2025b default ("midpoint" / R2007a / Type-5):
//   positions (k-0.5)/N for k=1..N → q = p*N + 0.5, clamped to [1, N].
double sliceQuantile(double *s, size_t n, double p)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    std::sort(s, s + n);
    if (n == 1) return s[0];
    const double q = p * static_cast<double>(n) + 0.5;
    if (q <= 1.0) return s[0];
    if (q >= static_cast<double>(n)) return s[n - 1];
    const size_t lo = static_cast<size_t>(std::floor(q)) - 1;
    const double frac = q - std::floor(q);
    return s[lo] + frac * (s[lo + 1] - s[lo]);
}

} // namespace

// ── bounds ────────────────────────────────────────────────────────────
std::tuple<Value, Value>
bounds(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "bounds");
    auto lo = applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) { return sliceMin(s, n); }, mr);
    auto hi = applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) { return sliceMax(s, n); }, mr);
    return std::make_tuple(std::move(lo), std::move(hi));
}

// ── iqr ───────────────────────────────────────────────────────────────
Value iqr(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "iqr");
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            std::vector<double> buf(s, s + n);
            const double q3 = sliceQuantile(buf.data(), n, 0.75);
            std::vector<double> buf2(s, s + n);
            const double q1 = sliceQuantile(buf2.data(), n, 0.25);
            return q3 - q1;
        }, mr);
}

namespace {

// k-largest along a generic dim. Output keeps the input shape but with
// the chosen dim shrunk to k. We allocate via createMatrix on the
// 2D / 3D fast path, and fall back to matrixND otherwise.
Value topKAlongDim(std::pmr::memory_resource *mr, const Value &x, int dim,
                   int kReq, bool ascending, const char *fn)
{
    if (kReq < 0)
        throw Error(std::string(fn) + ": k must be non-negative",
                     0, 0, fn, "", std::string("m:") + fn + ":badK");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Vector / scalar — single-slice fast path.
    if (x.dims().isVector() || x.isScalar()) {
        const size_t n = x.numel();
        const size_t k = std::min<size_t>(static_cast<size_t>(kReq), n);
        std::vector<double> buf(n);
        for (size_t i = 0; i < n; ++i) buf[i] = x.elemAsDouble(i);
        std::sort(buf.begin(), buf.end(),
                  [ascending](double a, double b) {
                      if (std::isnan(a)) return false;
                      if (std::isnan(b)) return true;
                      return ascending ? (a < b) : (a > b);
                  });
        // Output orientation matches input row/col-vector orientation.
        const bool isRow = (x.dims().rows() == 1);
        auto out = isRow
                    ? Value::matrix(1, k, ValueType::DOUBLE, mr)
                    : Value::matrix(k, 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < k; ++i) out.doubleDataMut()[i] = buf[i];
        return out;
    }

    const int d = resolveDim(x, dim, fn);
    const auto &dd = x.dims();
    if (dd.ndim() >= 4)
        throw Error(std::string(fn) + ": ND (rank>=4) input not yet supported",
                     0, 0, fn, "", std::string("m:") + fn + ":nd");

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t pageStride = R * C;
    size_t Ro = R, Co = C, Po = P;
    size_t sliceLen = 0;
    if (d == 1)      { sliceLen = R; Ro = std::min<size_t>(static_cast<size_t>(kReq), R); }
    else if (d == 2) { sliceLen = C; Co = std::min<size_t>(static_cast<size_t>(kReq), C); }
    else if (d == 3) { sliceLen = P; Po = std::min<size_t>(static_cast<size_t>(kReq), P); }
    else {
        // Out-of-rank → identity copy.
        return createLike(x, ValueType::DOUBLE, mr);
    }

    auto out = dd.is3D()
                ? Value::matrix3d(Ro, Co, Po, ValueType::DOUBLE, mr)
                : Value::matrix(Ro, Co, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();
    std::vector<double> buf(sliceLen);

    auto sortBuf = [&]() {
        std::sort(buf.begin(), buf.end(),
            [ascending](double a, double b) {
                if (std::isnan(a)) return false;
                if (std::isnan(b)) return true;
                return ascending ? (a < b) : (a > b);
            });
    };

    if (d == 1) {
        const size_t outPageStride = Ro * Co;
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c) {
                for (size_t r = 0; r < R; ++r)
                    buf[r] = src[p * pageStride + c * R + r];
                sortBuf();
                for (size_t i = 0; i < Ro; ++i)
                    dst[p * outPageStride + c * Ro + i] = buf[i];
            }
    } else if (d == 2) {
        const size_t outPageStride = Ro * Co;
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c)
                    buf[c] = src[p * pageStride + c * R + r];
                sortBuf();
                for (size_t i = 0; i < Co; ++i)
                    dst[p * outPageStride + i * Ro + r] = buf[i];
            }
    } else { // d == 3
        const size_t outPageStride = Ro * Co;
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                for (size_t p = 0; p < P; ++p)
                    buf[p] = src[p * pageStride + c * R + r];
                sortBuf();
                for (size_t i = 0; i < Po; ++i)
                    dst[i * outPageStride + c * Ro + r] = buf[i];
            }
    }
    return out;
}

} // namespace

// ── maxk / mink ───────────────────────────────────────────────────────
Value maxk(std::pmr::memory_resource *mr, const Value &x, int k, int dim)
{
    return topKAlongDim(mr, x, dim, k, /*ascending=*/false, "maxk");
}

Value mink(std::pmr::memory_resource *mr, const Value &x, int k, int dim)
{
    return topKAlongDim(mr, x, dim, k, /*ascending=*/true, "mink");
}

// ── rmse ──────────────────────────────────────────────────────────────
Value rmse(std::pmr::memory_resource *mr, const Value &f, const Value &a, int dim)
{
    if (f.dims() != a.dims() && !(f.isScalar() || a.isScalar()))
        throw Error("rmse: F and A must have compatible sizes",
                     0, 0, "rmse", "", "m:rmse:sizeMismatch");
    // Build the squared-difference array, then reduce.
    const size_t n = std::max(f.numel(), a.numel());
    auto diff = (f.numel() >= a.numel())
                  ? createLike(f, ValueType::DOUBLE, mr)
                  : createLike(a, ValueType::DOUBLE, mr);
    double *dst = diff.doubleDataMut();
    if (f.isScalar()) {
        const double fs = f.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double d = a.elemAsDouble(i) - fs;
            dst[i] = d * d;
        }
    } else if (a.isScalar()) {
        const double as = a.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double d = f.elemAsDouble(i) - as;
            dst[i] = d * d;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double d = f.elemAsDouble(i) - a.elemAsDouble(i);
            dst[i] = d * d;
        }
    }
    const int dResolved = resolveDim(diff, dim, "rmse");
    auto v = applyAlongDim(diff, dResolved,
        [](size_t, const double *s, size_t k) -> double {
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < k; ++i) sum += s[i];
            return std::sqrt(sum / static_cast<double>(k));
        }, mr);
    return v;
}

// ── mape ──────────────────────────────────────────────────────────────
Value mape(std::pmr::memory_resource *mr, const Value &f, const Value &a, int dim)
{
    if (f.dims() != a.dims() && !(f.isScalar() || a.isScalar()))
        throw Error("mape: F and A must have compatible sizes",
                     0, 0, "mape", "", "m:mape:sizeMismatch");
    const size_t n = std::max(f.numel(), a.numel());
    auto pct = (f.numel() >= a.numel())
                  ? createLike(f, ValueType::DOUBLE, mr)
                  : createLike(a, ValueType::DOUBLE, mr);
    double *dst = pct.doubleDataMut();

    if (f.isScalar()) {
        const double fs = f.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double ai = a.elemAsDouble(i);
            dst[i] = std::abs((ai - fs) / ai) * 100.0;
        }
    } else if (a.isScalar()) {
        const double as = a.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double fi = f.elemAsDouble(i);
            dst[i] = std::abs((as - fi) / as) * 100.0;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double fi = f.elemAsDouble(i);
            const double ai = a.elemAsDouble(i);
            dst[i] = std::abs((ai - fi) / ai) * 100.0;
        }
    }

    const int dResolved = resolveDim(pct, dim, "mape");
    return applyAlongDim(pct, dResolved,
        [](size_t, const double *s, size_t k) -> double {
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < k; ++i) sum += s[i];
            return sum / static_cast<double>(k);
        }, mr);
}

// ── isoutlier / rmoutliers / fillmissing / rmmissing / standardizeMissing ──

// isoutlier(x) — boolean array marking outliers via median + MAD
// (default MATLAB method: more than 3 scaled MADs from median).
Value isoutlier_of(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.numel() == 0) return Value::matrix(0, 0, ValueType::LOGICAL, mr);
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::LOGICAL, mr);
    uint8_t *od = out.logicalDataMut();
    const double *xd = x.doubleData();
    const std::size_t n = x.numel();

    std::vector<double> buf(xd, xd + n);
    std::sort(buf.begin(), buf.end());
    const double med = (n % 2 == 1) ? buf[n / 2]
                                     : 0.5 * (buf[n / 2 - 1] + buf[n / 2]);
    std::vector<double> dev(n);
    for (std::size_t i = 0; i < n; ++i) dev[i] = std::fabs(xd[i] - med);
    std::sort(dev.begin(), dev.end());
    const double mad = (n % 2 == 1) ? dev[n / 2]
                                     : 0.5 * (dev[n / 2 - 1] + dev[n / 2]);
    // MATLAB scales MAD by 1.4826 for normal-consistency.
    const double scaled_mad = mad * 1.4826;
    const double thresh = 3.0 * scaled_mad;

    for (std::size_t i = 0; i < n; ++i)
        od[i] = (std::fabs(xd[i] - med) > thresh) ? 1 : 0;
    return out;
}

// rmoutliers(x) — drop elements flagged by isoutlier; vector form.
Value rmoutliers_of(std::pmr::memory_resource *mr, const Value &x)
{
    auto mask = isoutlier_of(mr, x);
    const std::size_t n = x.numel();
    const double *xd = x.doubleData();
    const uint8_t *m = mask.logicalData();
    std::vector<double> kept;
    kept.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!m[i]) kept.push_back(xd[i]);
    const bool col = x.dims().ndim() >= 2 && x.dims().dim(1) == 1;
    auto out = col
        ? Value::matrix(kept.size(), 1, ValueType::DOUBLE, mr)
        : Value::matrix(1, kept.size(), ValueType::DOUBLE, mr);
    if (!kept.empty())
        std::copy(kept.begin(), kept.end(), out.doubleDataMut());
    return out;
}

// fillmissing(x, method[, constant_value]) — replace NaN with method.
// MATLAB-canonical methods: 'constant' (needs value), 'previous',
// 'next'. Internal 'mean'/'median' kept as a numkit convenience but
// undocumented (use mean(x,'omitnan') + 'constant' for portability).
Value fillmissing_of(std::pmr::memory_resource *mr, const Value &x,
                     const std::string &method, double constVal)
{
    const std::size_t n = x.numel();
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const double *xd = x.doubleData();
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::copy(xd, xd + n, od);

    if (method == "constant") {
        for (std::size_t i = 0; i < n; ++i)
            if (std::isnan(od[i])) od[i] = constVal;
        return out;
    }
    if (method == "mean" || method == "median") {
        std::vector<double> good;
        good.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            if (!std::isnan(xd[i])) good.push_back(xd[i]);
        if (good.empty()) return out;
        double fill;
        if (method == "mean") {
            double s = 0.0;
            for (double v : good) s += v;
            fill = s / static_cast<double>(good.size());
        } else {
            std::sort(good.begin(), good.end());
            const std::size_t gn = good.size();
            fill = (gn % 2 == 1) ? good[gn / 2]
                                  : 0.5 * (good[gn / 2 - 1] + good[gn / 2]);
        }
        for (std::size_t i = 0; i < n; ++i)
            if (std::isnan(od[i])) od[i] = fill;
        return out;
    }
    if (method == "previous") {
        double last_good = std::numeric_limits<double>::quiet_NaN();
        bool have = false;
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isnan(od[i])) { last_good = od[i]; have = true; }
            else if (have) od[i] = last_good;
        }
        return out;
    }
    if (method == "next") {
        double next_good = std::numeric_limits<double>::quiet_NaN();
        bool have = false;
        for (std::size_t ii = n; ii-- > 0;) {
            if (!std::isnan(od[ii])) { next_good = od[ii]; have = true; }
            else if (have) od[ii] = next_good;
        }
        return out;
    }
    throw Error("fillmissing: method must be 'constant', 'previous', "
                "'next', 'mean', or 'median' in this revision "
                "(MATLAB also supports 'nearest', 'linear', 'spline', "
                "'pchip', 'makima', 'movmean', 'movmedian', 'knn' -- "
                "those are deferred)",
                0, 0, "fillmissing", "", "m:fillmissing:method");
}

// rmmissing(x) — drop NaN entries.
Value rmmissing_of(std::pmr::memory_resource *mr, const Value &x)
{
    const std::size_t n = x.numel();
    const double *xd = x.doubleData();
    std::vector<double> kept;
    kept.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isnan(xd[i])) kept.push_back(xd[i]);
    const bool col = x.dims().ndim() >= 2 && x.dims().dim(1) == 1;
    auto out = col
        ? Value::matrix(kept.size(), 1, ValueType::DOUBLE, mr)
        : Value::matrix(1, kept.size(), ValueType::DOUBLE, mr);
    if (!kept.empty())
        std::copy(kept.begin(), kept.end(), out.doubleDataMut());
    return out;
}

// standardizeMissing(x, sentinel) — replace sentinel with NaN.
Value standardizeMissing_of(std::pmr::memory_resource *mr,
                            const Value &x, double sentinel)
{
    const std::size_t n = x.numel();
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
    if (n == 0) return out;
    const double *xd = x.doubleData();
    double *od = out.doubleDataMut();
    const double nanv = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t i = 0; i < n; ++i)
        od[i] = (xd[i] == sentinel) ? nanv : xd[i];
    return out;
}

// ── range / mad / geomean / harmmean / moment / trimmean ─────────────

// range(x) = max(x) - min(x) along dim.
Value range_of(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "range");
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            double lo = s[0], hi = s[0];
            for (size_t i = 1; i < n; ++i) {
                if (s[i] < lo) lo = s[i];
                if (s[i] > hi) hi = s[i];
            }
            return hi - lo;
        }, mr);
}

// mad(x) -- mean absolute deviation: mean(abs(x - mean(x))).
// mad(x, 1) -- median absolute deviation: median(abs(x - median(x))).
Value mad_of(std::pmr::memory_resource *mr, const Value &x, int flag, int dim)
{
    const int d = resolveDim(x, dim, "mad");
    if (flag == 0) {
        // Mean form.
        return applyAlongDim(x, d,
            [](size_t, const double *s, size_t n) -> double {
                if (n == 0) return std::numeric_limits<double>::quiet_NaN();
                double mean = 0.0;
                for (size_t i = 0; i < n; ++i) mean += s[i];
                mean /= static_cast<double>(n);
                double sum = 0.0;
                for (size_t i = 0; i < n; ++i) sum += std::fabs(s[i] - mean);
                return sum / static_cast<double>(n);
            }, mr);
    }
    // Median form.
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            std::vector<double> buf(s, s + n);
            const double med = sliceQuantile(buf.data(), n, 0.5);
            std::vector<double> dev(n);
            for (size_t i = 0; i < n; ++i) dev[i] = std::fabs(s[i] - med);
            return sliceQuantile(dev.data(), n, 0.5);
        }, mr);
}

// geomean(x) = (prod(x))^(1/n) = exp(mean(log(x))). x must be >= 0.
Value geomean_of(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "geomean");
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < n; ++i) {
                if (s[i] < 0.0)
                    return std::numeric_limits<double>::quiet_NaN();
                if (s[i] == 0.0) return 0.0;
                sum += std::log(s[i]);
            }
            return std::exp(sum / static_cast<double>(n));
        }, mr);
}

// harmmean(x) = n / sum(1./x). x must be > 0.
Value harmmean_of(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "harmmean");
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < n; ++i) {
                if (s[i] <= 0.0)
                    return std::numeric_limits<double>::quiet_NaN();
                sum += 1.0 / s[i];
            }
            return static_cast<double>(n) / sum;
        }, mr);
}

// moment(x, k) = mean((x - mean(x))^k). k >= 2.
Value moment_of(std::pmr::memory_resource *mr, const Value &x, int order, int dim)
{
    const int d = resolveDim(x, dim, "moment");
    const int k = order;
    return applyAlongDim(x, d,
        [k](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            if (k < 0) return std::numeric_limits<double>::quiet_NaN();
            if (k == 0) return 1.0;          // m_0 = 1
            if (k == 1) return 0.0;          // central first moment
            double mean = 0.0;
            for (size_t i = 0; i < n; ++i) mean += s[i];
            mean /= static_cast<double>(n);
            double sum = 0.0;
            for (size_t i = 0; i < n; ++i) {
                const double d2 = s[i] - mean;
                sum += std::pow(d2, k);
            }
            return sum / static_cast<double>(n);
        }, mr);
}

// trimmean(x, p) = mean of x after trimming p/2% from each end (p in [0, 100]).
Value trimmean_of(std::pmr::memory_resource *mr, const Value &x, double pct, int dim)
{
    if (pct < 0.0 || pct >= 100.0)
        throw Error("trimmean: percent must be in [0, 100)",
                    0, 0, "trimmean", "", "m:trimmean:badPct");
    const int d = resolveDim(x, dim, "trimmean");
    const double p = pct;
    return applyAlongDim(x, d,
        [p](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            // Number of values to trim from EACH end.
            const size_t k = static_cast<size_t>(std::floor(
                static_cast<double>(n) * p / 200.0));
            if (2 * k >= n) return std::numeric_limits<double>::quiet_NaN();
            std::vector<double> buf(s, s + n);
            std::sort(buf.begin(), buf.end());
            double sum = 0.0;
            for (size_t i = k; i < n - k; ++i) sum += buf[i];
            return sum / static_cast<double>(n - 2 * k);
        }, mr);
}

// ── ksdensity ─────────────────────────────────────────────────────────

namespace {
// Per-kernel pdf K(u) and cdf F(u). All normalized so ∫K = 1 and the
// kernel is supported on [-1, 1] for finite-support kernels (or all of
// R for normal). Bandwidth is applied externally.
enum class KsKernel { Normal, Box, Triangle, Epanechnikov };
KsKernel parse_ks_kernel(const std::string &raw) {
    std::string s; s.reserve(raw.size());
    for (char c : raw) s.push_back((char)std::tolower((unsigned char)c));
    if (s == "normal" || s == "gauss" || s == "gaussian") return KsKernel::Normal;
    if (s == "box" || s == "rectangular" || s == "rect")   return KsKernel::Box;
    if (s == "triangle" || s == "triangular")              return KsKernel::Triangle;
    if (s == "epanechnikov" || s == "epan")                return KsKernel::Epanechnikov;
    throw Error("ksdensity: unknown Kernel '" + raw + "'",
                0, 0, "ksdensity", "", "m:ksdensity:kernel");
}
inline double ks_pdf(double u, KsKernel k) {
    switch (k) {
        case KsKernel::Normal:
            return 0.3989422804014327 * std::exp(-0.5 * u * u);
        case KsKernel::Box:
            return (std::fabs(u) <= 1.0) ? 0.5 : 0.0;
        case KsKernel::Triangle:
            return (std::fabs(u) <= 1.0) ? (1.0 - std::fabs(u)) : 0.0;
        case KsKernel::Epanechnikov:
            return (std::fabs(u) <= 1.0) ? (0.75 * (1.0 - u * u)) : 0.0;
    }
    return 0.0;
}
// MATLAB-compat scaling: each kernel's "unit" form has variance σ² which
// differs across kernel types. MATLAB normalizes the EFFECTIVE bandwidth
// so that h has the same standard-deviation interpretation as the
// normal kernel. Result: multiply h by 1/σ_unit for finite-support
// kernels.
//   Normal: σ²=1     → factor 1.0000
//   Box:    σ²=1/3   → factor sqrt(3) ≈ 1.7321
//   Tri:    σ²=1/6   → factor sqrt(6) ≈ 2.4495
//   Epan:   σ²=1/5   → factor sqrt(5) ≈ 2.2361
inline double ks_h_factor(KsKernel k) {
    switch (k) {
        case KsKernel::Normal:       return 1.0;
        case KsKernel::Box:          return std::sqrt(3.0);
        case KsKernel::Triangle:     return std::sqrt(6.0);
        case KsKernel::Epanechnikov: return std::sqrt(5.0);
    }
    return 1.0;
}
inline double ks_cdf(double u, KsKernel k) {
    switch (k) {
        case KsKernel::Normal:
            return 0.5 * (1.0 + std::erf(u / std::sqrt(2.0)));
        case KsKernel::Box:
            if (u <= -1.0) return 0.0;
            if (u >=  1.0) return 1.0;
            return 0.5 * (u + 1.0);
        case KsKernel::Triangle:
            if (u <= -1.0) return 0.0;
            if (u >=  1.0) return 1.0;
            if (u <= 0.0) return 0.5 * (u + 1.0) * (u + 1.0);
            return 1.0 - 0.5 * (1.0 - u) * (1.0 - u);
        case KsKernel::Epanechnikov:
            if (u <= -1.0) return 0.0;
            if (u >=  1.0) return 1.0;
            return 0.5 + 0.75 * u - 0.25 * u * u * u;
    }
    return 0.0;
}
} // anonymous

// Result struct for the extended ksdensity API. Forward-declared also
// above the engine-adapter `ksdensity_reg` (in the outer namespace).
struct KsdensityFull { Value f, xi, bw; };

KsdensityFull
ksdensity_full(std::pmr::memory_resource *mr,
               const Value &x, const Value &pts,
               double bw_user, const std::string &kernel_name,
               const std::string &function_mode,
               size_t numpoints,
               const Value *weights)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0)
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::scalar(nan, mr)};

    const KsKernel kernel = parse_ks_kernel(kernel_name);

    std::vector<double> xv(N);
    std::vector<double> wv(N, 1.0);
    if (weights && weights->numel() == N)
        for (size_t i = 0; i < N; ++i) wv[i] = weights->elemAsDouble(i);
    for (size_t i = 0; i < N; ++i) xv[i] = x.elemAsDouble(i);
    // Normalize weights so Σw = 1 (matches MATLAB semantics).
    double Wsum = 0.0;
    for (double w : wv) Wsum += w;
    if (!(Wsum > 0.0)) Wsum = double(N);
    const double Winv = 1.0 / Wsum;

    // Sort xv and wv jointly.
    std::vector<size_t> idx(N);
    for (size_t i = 0; i < N; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(),
              [&](size_t a, size_t b) { return xv[a] < xv[b]; });
    std::vector<double> xs(N), ws(N);
    for (size_t i = 0; i < N; ++i) { xs[i] = xv[idx[i]]; ws[i] = wv[idx[i]]; }

    // MATLAB's default bandwidth for the normal kernel:
    //   sigma = mad(x, 1) / 0.6745 if positive, else iqr(x) / 1.349
    //   bw    = sigma · (4 / (3·n))^(1/5)
    double bw = bw_user;
    if (!(bw > 0.0)) {
        // median absolute deviation
        std::vector<double> tmp = xs;          // already sorted
        const double med = (N % 2 == 1) ? tmp[N / 2]
                                        : 0.5 * (tmp[N / 2 - 1] + tmp[N / 2]);
        std::vector<double> dev(N);
        for (size_t i = 0; i < N; ++i) dev[i] = std::fabs(xs[i] - med);
        std::sort(dev.begin(), dev.end());
        const double mad = (N % 2 == 1) ? dev[N / 2]
                                        : 0.5 * (dev[N / 2 - 1] + dev[N / 2]);
        double sigma = (mad > 0.0) ? mad / 0.6745 : 0.0;
        if (!(sigma > 0.0)) {
            // IQR fallback.
            auto pct = [&](double p) {
                const double pos = p * (double(N) - 1.0);
                const size_t lo = (size_t)std::floor(pos);
                const size_t hi = (size_t)std::ceil(pos);
                const double t = pos - double(lo);
                return xs[lo] * (1.0 - t) + xs[hi] * t;
            };
            const double iqr = pct(0.75) - pct(0.25);
            sigma = (iqr > 0.0) ? iqr / 1.349 : 1.0;
        }
        bw = sigma * std::pow(4.0 / (3.0 * double(N)), 0.2);
        if (!(bw > 0.0)) bw = 1.0;
    }

    // Apply kernel-specific bandwidth scaling so h has consistent
    // standard-deviation semantics across kernels (MATLAB convention).
    const double h_eff = bw * ks_h_factor(kernel);

    // Build evaluation grid.
    std::vector<double> grid;
    if (pts.isEmpty()) {
        const size_t M = (numpoints > 0) ? numpoints : 100;
        const double xmin = xs.front() - 3.0 * h_eff;
        const double xmax = xs.back()  + 3.0 * h_eff;
        grid.resize(M);
        if (M == 1) grid[0] = xmin;
        else {
            const double step = (xmax - xmin) / double(M - 1);
            for (size_t i = 0; i < M; ++i) grid[i] = xmin + step * double(i);
            grid[M - 1] = xmax;
        }
    } else {
        const size_t M = pts.numel();
        grid.resize(M);
        for (size_t i = 0; i < M; ++i) grid[i] = pts.elemAsDouble(i);
    }

    const size_t M = grid.size();
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::string mode = function_mode.empty() ? "pdf"
                                                    : lower(function_mode);
    Value fv = Value::matrix(1, M, ValueType::DOUBLE, mr);
    double *fd = fv.doubleDataMut();
    const double inv_h = 1.0 / h_eff;

    if (mode == "pdf") {
        for (size_t j = 0; j < M; ++j) {
            double sum = 0.0;
            for (size_t i = 0; i < N; ++i) {
                const double u = (grid[j] - xs[i]) * inv_h;
                sum += ws[i] * ks_pdf(u, kernel);
            }
            fd[j] = sum * inv_h * Winv;
        }
    } else if (mode == "cdf" || mode == "survivor" || mode == "cumhazard"
               || mode == "cumulative hazard") {
        for (size_t j = 0; j < M; ++j) {
            double sum = 0.0;
            for (size_t i = 0; i < N; ++i) {
                const double u = (grid[j] - xs[i]) * inv_h;
                sum += ws[i] * ks_cdf(u, kernel);
            }
            const double F = sum * Winv;
            if      (mode == "cdf")      fd[j] = F;
            else if (mode == "survivor") fd[j] = 1.0 - F;
            else                         fd[j] = -std::log(std::max(1.0 - F, 1e-300));
        }
    } else if (mode == "icdf") {
        throw Error("ksdensity: 'Function'='icdf' is not yet supported",
                    0, 0, "ksdensity", "", "m:ksdensity:icdf_nyi");
    } else {
        throw Error("ksdensity: unknown Function '" + mode + "'",
                    0, 0, "ksdensity", "", "m:ksdensity:badfn");
    }

    Value xiV = Value::matrix(1, M, ValueType::DOUBLE, mr);
    double *xd = xiV.doubleDataMut();
    for (size_t i = 0; i < M; ++i) xd[i] = grid[i];
    return {std::move(fv), std::move(xiV), Value::scalar(bw, mr)};
}

// Backward-compat 4-arg form: pdf with normal kernel, default
// numpoints=100, no weights.
std::tuple<Value, Value, Value>
ksdensity(std::pmr::memory_resource *mr, const Value &x, const Value &pts,
          double bw_user)
{
    auto R = ksdensity_full(mr, x, pts, bw_user, "normal", "pdf", 100, nullptr);
    return std::make_tuple(std::move(R.f), std::move(R.xi), std::move(R.bw));
}

// ── prepareCurveData / prepareSurfaceData ─────────────────────────────

namespace {

inline bool finite_double(double v) {
    return !std::isnan(v) && !std::isinf(v);
}

// Flatten + filter helper. `srcs` give pointer/numel pairs (already
// linearised), `keep_mask[i]` indicates whether row i survives.
Value pack_filtered(std::pmr::memory_resource *mr,
                    const std::vector<double> &src,
                    const std::vector<uint8_t> &keep)
{
    size_t kept = 0;
    for (uint8_t k : keep) if (k) ++kept;
    Value out = Value::matrix(kept, 1, ValueType::DOUBLE, mr);
    if (kept == 0) return out;
    double *od = out.doubleDataMut();
    size_t j = 0;
    for (size_t i = 0; i < src.size(); ++i)
        if (keep[i]) od[j++] = src[i];
    return out;
}

} // anonymous

std::tuple<Value, Value, Value>
prepareCurveData(std::pmr::memory_resource *mr,
                 const Value &x, const Value &y, const Value &w)
{
    const size_t Nx = x.numel();
    const size_t Ny = y.numel();
    const bool   hasW = !w.isEmpty();
    const size_t Nw = hasW ? w.numel() : Nx;
    if (Nx != Ny || (hasW && Nw != Nx))
        throw Error("prepareCurveData: x, y" +
                    std::string(hasW ? ", w" : "") + " must be same length",
                    0, 0, "prepareCurveData", "", "m:prepCD:size");

    std::vector<double> xv(Nx), yv(Nx), wv(hasW ? Nx : 0);
    std::vector<uint8_t> keep(Nx, 1);
    for (size_t i = 0; i < Nx; ++i) {
        xv[i] = x.elemAsDouble(i);
        yv[i] = y.elemAsDouble(i);
        if (hasW) wv[i] = w.elemAsDouble(i);
        if (!finite_double(xv[i]) || !finite_double(yv[i]) ||
            (hasW && !finite_double(wv[i])))
            keep[i] = 0;
    }

    Value xo = pack_filtered(mr, xv, keep);
    Value yo = pack_filtered(mr, yv, keep);
    Value wo = hasW ? pack_filtered(mr, wv, keep)
                    : Value::matrix(0, 1, ValueType::DOUBLE, mr);
    return {std::move(xo), std::move(yo), std::move(wo)};
}

std::tuple<Value, Value, Value>
prepareSurfaceData(std::pmr::memory_resource *mr,
                   const Value &x, const Value &y, const Value &z)
{
    // For surface fits MATLAB lets x and y be either vectors of length
    // numel(z), or matrices the same shape as z (meshgrid). Normalise
    // by linearising in column-major order; numel must match.
    const size_t Nz = z.numel();
    const size_t Nx = x.numel();
    const size_t Ny = y.numel();

    auto broadcastTo = [&](const Value &v, size_t target) -> std::vector<double> {
        std::vector<double> out(target);
        if (v.numel() == target) {
            for (size_t i = 0; i < target; ++i) out[i] = v.elemAsDouble(i);
        } else {
            // Allow x = row of length cols, y = col of length rows for
            // implicit meshgrid (matches MATLAB behaviour).
            const auto &dz = z.dims();
            const size_t rows = dz.rows();
            const size_t cols = dz.cols();
            if (v.numel() == cols) {
                // treat as x-coords per column
                for (size_t c = 0; c < cols; ++c)
                    for (size_t r = 0; r < rows; ++r)
                        out[r + c * rows] = v.elemAsDouble(c);
            } else if (v.numel() == rows) {
                for (size_t c = 0; c < cols; ++c)
                    for (size_t r = 0; r < rows; ++r)
                        out[r + c * rows] = v.elemAsDouble(r);
            } else {
                throw Error("prepareSurfaceData: x, y, z size mismatch",
                            0, 0, "prepareSurfaceData", "", "m:prepSD:size");
            }
        }
        return out;
    };

    std::vector<double> xv = broadcastTo(x, Nz);
    std::vector<double> yv = broadcastTo(y, Nz);
    std::vector<double> zv(Nz);
    for (size_t i = 0; i < Nz; ++i) zv[i] = z.elemAsDouble(i);

    std::vector<uint8_t> keep(Nz, 1);
    for (size_t i = 0; i < Nz; ++i) {
        if (!finite_double(xv[i]) || !finite_double(yv[i]) ||
            !finite_double(zv[i]))
            keep[i] = 0;
    }

    Value xo = pack_filtered(mr, xv, keep);
    Value yo = pack_filtered(mr, yv, keep);
    Value zo = pack_filtered(mr, zv, keep);
    (void)Nx; (void)Ny;
    return {std::move(xo), std::move(yo), std::move(zo)};
}

// ── datastats ─────────────────────────────────────────────────────────

std::tuple<Value, Value, Value, Value, Value, Value, Value>
datastats(std::pmr::memory_resource *mr, const Value &x)
{
    const size_t N = x.numel();
    std::vector<double> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i) v.push_back(x.elemAsDouble(i));

    const auto nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) {
        return std::make_tuple(Value::scalar(0.0, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr));
    }

    double mn = v[0], mx = v[0], sum = 0.0;
    for (double vi : v) {
        if (vi < mn) mn = vi;
        if (vi > mx) mx = vi;
        sum += vi;
    }
    const double mean = sum / static_cast<double>(N);

    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    double median;
    if (N % 2 == 1) median = sorted[N / 2];
    else            median = 0.5 * (sorted[N / 2 - 1] + sorted[N / 2]);

    double sd = 0.0;
    if (N > 1) {
        double sq = 0.0;
        for (double vi : v) { const double d = vi - mean; sq += d * d; }
        sd = std::sqrt(sq / static_cast<double>(N - 1));
    }
    const double range = mx - mn;

    return std::make_tuple(Value::scalar(static_cast<double>(N), mr),
                           Value::scalar(mx,     mr),
                           Value::scalar(mn,     mr),
                           Value::scalar(mean,   mr),
                           Value::scalar(median, mr),
                           Value::scalar(range,  mr),
                           Value::scalar(sd,     mr));
}

// Forward declarations for ecdf (defined at the end of this TU).
struct EcdfFull { Value f, x, flo, fup; };
EcdfFull ecdf_full(std::pmr::memory_resource *mr,
                   const Value &y, const Value *freq,
                   const std::string &function_mode, double alpha,
                   bool want_bounds);

// Forward declaration for ksdensity_full. KsdensityFull is defined above.
struct KsdensityFull;
KsdensityFull ksdensity_full(std::pmr::memory_resource *mr,
                             const Value &x, const Value &pts,
                             double bw_user, const std::string &kernel_name,
                             const std::string &function_mode,
                             size_t numpoints, const Value *weights);

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void prepareCurveData_reg(Span<const Value> args, size_t nargout,
                          Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("prepareCurveData: requires (X, Y[, W])",
                    0, 0, "prepareCurveData", "", "m:prepCD:nargin");
    auto *mr = ctx.engine->resource();
    Value w_empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &w = (args.size() >= 3) ? args[2] : w_empty;
    auto [xo, yo, wo] = prepareCurveData(mr, args[0], args[1], w);
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(wo);
}

void prepareSurfaceData_reg(Span<const Value> args, size_t nargout,
                            Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("prepareSurfaceData: requires (X, Y, Z)",
                    0, 0, "prepareSurfaceData", "", "m:prepSD:nargin");
    auto [xo, yo, zo] = prepareSurfaceData(ctx.engine->resource(),
                                            args[0], args[1], args[2]);
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(zo);
}

void ksdensity_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ksdensity: requires (x[, pts][, N-V pairs])",
                    0, 0, "ksdensity", "", "m:ksdensity:nargin");
    auto *mr = ctx.engine->resource();
    Value pts = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    double bw_user = 0.0;
    std::string kernel = "normal";
    std::string function_mode = "pdf";
    size_t numpoints = 100;
    const Value *weights = nullptr;
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    size_t i = 1;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        pts = args[i];
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        const std::string name = lower(args[i].toString());
        const Value &v = args[i + 1];
        if      (name == "bandwidth" || name == "width") {
            if (v.isChar() || v.isString()) {
                // 'normal-approx' / 'plug-in' string forms — only
                // 'normal-approx' (default behavior) is supported.
                const std::string s = lower(v.toString());
                if (s != "normal-approx" && s != "plug-in")
                    throw Error("ksdensity: unknown Bandwidth string '" + s + "'",
                                0, 0, "ksdensity", "", "m:ksdensity:bw");
                bw_user = 0.0;
            } else {
                bw_user = v.toScalar();
            }
        }
        else if (name == "kernel")    kernel = v.toString();
        else if (name == "function")  function_mode = v.toString();
        else if (name == "numpoints") numpoints = (size_t)v.toScalar();
        else if (name == "weights")   { if (!v.isEmpty()) weights = &v; }
        else if (name == "censoring" || name == "support"
                 || name == "boundarycorrection") {
            if (!v.isEmpty())
                throw Error("ksdensity: '" + name + "' is not yet supported",
                            0, 0, "ksdensity", "", "m:ksdensity:nyi");
        }
        // 'PlotFcn' silently ignored (no-op headless).
        i += 2;
    }
    auto R = ksdensity_full(mr, args[0], pts, bw_user, kernel,
                            function_mode, numpoints, weights);
    outs[0] = std::move(R.f);
    if (nargout > 1) outs[1] = std::move(R.xi);
    if (nargout > 2) outs[2] = std::move(R.bw);
}

void datastats_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("datastats: requires X[, Y]",
                    0, 0, "datastats", "", "m:datastats:nargin");
    auto build = [&](const Value &v) {
        auto [num, mx, mn, me, md, rg, sd] =
            datastats(ctx.engine->resource(), v);
        Value s = Value::structure(ctx.engine->resource());
        s.field("num")    = num;
        s.field("max")    = mx;
        s.field("min")    = mn;
        s.field("mean")   = me;
        s.field("median") = md;
        s.field("range")  = rg;
        s.field("std")    = sd;
        return s;
    };
    outs[0] = build(args[0]);
    // Two-arg form returns separate stats structs for x and y.
    // We only fill outs[1] if a second argument was supplied AND the
    // caller actually requested two outputs (otherwise it's a no-op).
    if (args.size() >= 2 && outs.size() > 1)
        outs[1] = build(args[1]);
}

void bounds_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bounds: requires at least 1 argument",
                     0, 0, "bounds", "", "m:bounds:nargin");
    int dim = 0;
    bool flatten = false;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &a = args[1];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (s == "all") flatten = true;
            else throw Error("bounds: unknown flag '" + s + "'",
                             0, 0, "bounds", "", "m:bounds:badFlag");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim: full-flatten only
            std::vector<int> dims;
            for (size_t i = 0; i < a.numel(); ++i)
                dims.push_back(static_cast<int>(a.elemAsDouble(i)));
            const int rank = args[0].dims().is3D() ? 3
                              : (args[0].dims().isVector() || args[0].isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error("bounds: vecdim entries out of range",
                                0, 0, "bounds", "", "m:bounds:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("bounds: partial vecdim reduction is not yet "
                            "supported (only full-flatten vecdim)",
                            0, 0, "bounds", "", "m:bounds:vecdim");
            flatten = true;
        }
    }
    auto *mr = ctx.engine->resource();
    if (flatten) {
        Value flat = Value::matrix(1, args[0].numel(), ValueType::DOUBLE, mr);
        if (args[0].numel() > 0) {
            const double *src = args[0].doubleData();
            std::copy(src, src + args[0].numel(), flat.doubleDataMut());
        }
        auto [lo, hi] = bounds(mr, flat, 2);
        outs[0] = std::move(lo);
        if (nargout > 1) outs[1] = std::move(hi);
    } else {
        auto [lo, hi] = bounds(mr, args[0], dim);
        outs[0] = std::move(lo);
        if (nargout > 1) outs[1] = std::move(hi);
    }
}

void iqr_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iqr: requires at least 1 argument",
                     0, 0, "iqr", "", "m:iqr:nargin");
    int dim = 0;
    bool flatten = false;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &a = args[1];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "all") flatten = true;
            else throw Error("iqr: unknown flag '" + s + "'",
                              0, 0, "iqr", "", "m:iqr:badFlag");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim: only full-flatten coverage supported
            std::vector<int> dims;
            for (size_t i = 0; i < a.numel(); ++i)
                dims.push_back(static_cast<int>(a.elemAsDouble(i)));
            const int rank = args[0].dims().is3D() ? 3
                              : (args[0].dims().isVector() || args[0].isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error("iqr: vecdim entries out of range",
                                0, 0, "iqr", "", "m:iqr:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("iqr: partial vecdim reduction is not yet "
                            "supported (only full-flatten vecdim like [1 2])",
                            0, 0, "iqr", "", "m:iqr:vecdim");
            flatten = true;
        }
    }
    auto *mr = ctx.engine->resource();
    if (flatten) {
        // Flatten and compute on the 1×N row.
        Value flat = Value::matrix(1, args[0].numel(), ValueType::DOUBLE, mr);
        if (args[0].numel() > 0) {
            const double *src = args[0].doubleData();
            std::copy(src, src + args[0].numel(), flat.doubleDataMut());
        }
        outs[0] = iqr(mr, flat, 2);
    } else {
        outs[0] = iqr(mr, args[0], dim);
    }
}

void maxk_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("maxk: requires at least 2 arguments (x, k)",
                     0, 0, "maxk", "", "m:maxk:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    int dim = 0;
    // Optional positional dim (numeric scalar that's not a string).
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    // Remaining args may be Name-Value pairs; only 'ComparisonMethod'
    // is documented (real|abs|auto). All real-valued operations match
    // 'auto' = 'real' since complex maxk on real input is identical.
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("maxk: expected Name-Value pair",
                        0, 0, "maxk", "", "m:maxk:nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (name == "comparisonmethod") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (m != "real" && m != "abs" && m != "auto")
                throw Error("maxk: ComparisonMethod must be 'real', 'abs' or 'auto'",
                            0, 0, "maxk", "", "m:maxk:cm");
            // For real input 'auto'/'real' identical; 'abs' is a parity gap.
            if (m == "abs")
                throw Error("maxk: ComparisonMethod='abs' not yet supported",
                            0, 0, "maxk", "", "m:maxk:cmAbs");
        } else {
            throw Error("maxk: unknown Name-Value '" + name + "'",
                        0, 0, "maxk", "", "m:maxk:nv");
        }
        i += 2;
    }
    outs[0] = maxk(ctx.engine->resource(), args[0], k, dim);
}

void mink_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mink: requires at least 2 arguments (x, k)",
                     0, 0, "mink", "", "m:mink:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    int dim = 0;
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("mink: expected Name-Value pair",
                        0, 0, "mink", "", "m:mink:nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (name == "comparisonmethod") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (m != "real" && m != "abs" && m != "auto")
                throw Error("mink: ComparisonMethod must be 'real', 'abs' or 'auto'",
                            0, 0, "mink", "", "m:mink:cm");
            if (m == "abs")
                throw Error("mink: ComparisonMethod='abs' not yet supported",
                            0, 0, "mink", "", "m:mink:cmAbs");
        } else {
            throw Error("mink: unknown Name-Value '" + name + "'",
                        0, 0, "mink", "", "m:mink:nv");
        }
        i += 2;
    }
    outs[0] = mink(ctx.engine->resource(), args[0], k, dim);
}

// Common parser for mape/rmse trailing args: optional dim ('all', vecdim,
// integer scalar). Returns (dim, flatten). Vector inputs to mape/rmse
// are inherently 1-D so flatten and dim=0 produce the same result.
namespace {
void parseDimOrAll(const Value &x, Span<const Value> args, size_t pos,
                   int &dim, bool &flatten, const char *fn)
{
    dim = 0; flatten = false;
    if (pos >= args.size() || args[pos].isEmpty()) return;
    const Value &a = args[pos];
    if (a.isChar() || a.isString()) {
        std::string s = a.toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (s == "all") { flatten = true; return; }
        throw Error(std::string(fn) + ": unknown flag '" + s + "'",
                    0, 0, fn, "", std::string("m:") + fn + ":badFlag");
    }
    if (a.numel() == 1) { dim = static_cast<int>(a.toScalar()); return; }
    // vecdim — full-flatten only
    const int rank = x.dims().is3D() ? 3
                      : (x.dims().isVector() || x.isScalar() ? 1 : 2);
    std::vector<bool> seen(rank + 1, false);
    for (size_t i = 0; i < a.numel(); ++i) {
        int d = static_cast<int>(a.elemAsDouble(i));
        if (d < 1 || d > rank)
            throw Error(std::string(fn) + ": vecdim entries out of range",
                        0, 0, fn, "", std::string("m:") + fn + ":vecdim");
        seen[d] = true;
    }
    bool allCovered = true;
    for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
    if (!allCovered)
        throw Error(std::string(fn) + ": partial vecdim reduction not supported",
                    0, 0, fn, "", std::string("m:") + fn + ":vecdim");
    flatten = true;
}

Value flattenToRow(std::pmr::memory_resource *mr, const Value &x)
{
    Value flat = Value::matrix(1, x.numel(), ValueType::DOUBLE, mr);
    if (x.numel() > 0) {
        const double *src = x.doubleData();
        std::copy(src, src + x.numel(), flat.doubleDataMut());
    }
    return flat;
}
} // anonymous

void mape_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mape: requires 2 arguments (F, A)",
                     0, 0, "mape", "", "m:mape:nargin");
    int dim = 0; bool flatten = false;
    parseDimOrAll(args[0], args, 2, dim, flatten, "mape");
    auto *mr = ctx.engine->resource();
    if (flatten) {
        outs[0] = mape(mr, flattenToRow(mr, args[0]), flattenToRow(mr, args[1]), 2);
    } else {
        outs[0] = mape(mr, args[0], args[1], dim);
    }
}

void rmse_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rmse: requires at least 2 arguments (F, A)",
                     0, 0, "rmse", "", "m:rmse:nargin");
    int dim = 0; bool flatten = false;
    parseDimOrAll(args[0], args, 2, dim, flatten, "rmse");
    auto *mr = ctx.engine->resource();
    if (flatten) {
        outs[0] = rmse(mr, flattenToRow(mr, args[0]), flattenToRow(mr, args[1]), 2);
    } else {
        outs[0] = rmse(mr, args[0], args[1], dim);
    }
}

void ecdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ecdf: requires (y[, N-V pairs])",
                     0, 0, "ecdf", "", "m:ecdf:nargin");
    auto *mr = ctx.engine->resource();
    std::string function_mode = "cdf";
    double alpha = 0.05;
    const Value *freq = nullptr;
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) break;
        const std::string key = lower(args[i].toString());
        const Value &v = args[i + 1];
        if      (key == "function")  function_mode = v.toString();
        else if (key == "frequency") {
            if (!v.isEmpty()) freq = &v;
        }
        else if (key == "alpha")     alpha = v.toScalar();
        else if (key == "censoring") {
            if (!v.isEmpty())
                throw Error("ecdf: 'Censoring' is not yet supported "
                            "(Kaplan-Meier estimator). Skip the arg or "
                            "filter censored observations beforehand.",
                            0, 0, "ecdf", "", "m:ecdf:censoring_nyi");
        }
        else if (key == "iterationlimit" || key == "tolerance"
                 || key == "icmfrequency" || key == "bounds") {
            // Silently accepted (no-op for non-censored ecdf).
        }
    }
    const bool want_bounds = (nargout > 2);
    auto R = ecdf_full(mr, args[0], freq, function_mode, alpha, want_bounds);
    outs[0] = std::move(R.f);
    if (nargout > 1) outs[1] = std::move(R.x);
    if (nargout > 2) outs[2] = std::move(R.flo);
    if (nargout > 3) outs[3] = std::move(R.fup);
}

void ecdfhist_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ecdfhist: requires (f, x [, m])",
                     0, 0, "ecdfhist", "", "m:ecdfhist:nargin");
    int m = 10;
    if (args.size() >= 3 && !args[2].isEmpty())
        m = static_cast<int>(args[2].toScalar());
    auto [n, c] = ecdfhist(ctx.engine->resource(), args[0], args[1], m);
    outs[0] = std::move(n);
    if (nargout > 1) outs[1] = std::move(c);
}

// ── missing-data adapters ────────────────────────────────────────────

void isoutlier_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isoutlier: requires at least 1 argument",
                    0, 0, "isoutlier", "", "m:isoutlier:nargin");
    outs[0] = isoutlier_of(ctx.engine->resource(), args[0]);
}

void rmoutliers_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rmoutliers: requires at least 1 argument",
                    0, 0, "rmoutliers", "", "m:rmoutliers:nargin");
    outs[0] = rmoutliers_of(ctx.engine->resource(), args[0]);
}

void fillmissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fillmissing: requires (x, method[, constant_value])",
                    0, 0, "fillmissing", "", "m:fillmissing:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("fillmissing: method must be a string",
                    0, 0, "fillmissing", "", "m:fillmissing:method");
    const std::string m = args[1].toString();
    const double cv = (args.size() >= 3) ? args[2].toScalar() : 0.0;
    outs[0] = fillmissing_of(ctx.engine->resource(), args[0], m, cv);
}

void rmmissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rmmissing: requires at least 1 argument",
                    0, 0, "rmmissing", "", "m:rmmissing:nargin");
    outs[0] = rmmissing_of(ctx.engine->resource(), args[0]);
}

void standardizeMissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("standardizeMissing: requires (x, sentinel)",
                    0, 0, "standardizeMissing", "", "m:standardizeMissing:nargin");
    outs[0] = standardizeMissing_of(ctx.engine->resource(), args[0], args[1].toScalar());
}

// ── range / mad / geomean / harmmean / moment / trimmean adapters ────

void range_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("range: requires at least 1 argument",
                    0, 0, "range", "", "m:range:nargin");
    const int dim = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = range_of(ctx.engine->resource(), args[0], dim);
}

void mad_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mad: requires at least 1 argument",
                    0, 0, "mad", "", "m:mad:nargin");
    const int flag = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    const int dim  = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = mad_of(ctx.engine->resource(), args[0], flag, dim);
}

void geomean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("geomean: requires at least 1 argument",
                    0, 0, "geomean", "", "m:geomean:nargin");
    const int dim = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = geomean_of(ctx.engine->resource(), args[0], dim);
}

void harmmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("harmmean: requires at least 1 argument",
                    0, 0, "harmmean", "", "m:harmmean:nargin");
    const int dim = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = harmmean_of(ctx.engine->resource(), args[0], dim);
}

void moment_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("moment: requires (x, order)",
                    0, 0, "moment", "", "m:moment:nargin");
    const int order = static_cast<int>(args[1].toScalar());
    const int dim   = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = moment_of(ctx.engine->resource(), args[0], order, dim);
}

void trimmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("trimmean: requires (x, percent)",
                    0, 0, "trimmean", "", "m:trimmean:nargin");
    const double pct = args[1].toScalar();
    const int dim    = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = trimmean_of(ctx.engine->resource(), args[0], pct, dim);
}

} // namespace detail

// ── ecdfhist ─────────────────────────────────────────────────────────
// Convert (f, x) from ecdf into a probability-density histogram.
// Algorithm: probs[k] = f[k+1] - f[k]; vals[k] = x[k+1] (the unique
// data values). Bin into m equal-width bins over [min(x), max(x)],
// each height = sum(probs in bin) / bin_width.
std::tuple<Value, Value>
ecdfhist(std::pmr::memory_resource *mr, const Value &f, const Value &x, int m)
{
    if (m < 1)
        throw Error("ecdfhist: number of bins must be >= 1",
                    0, 0, "ecdfhist", "", "m:ecdfhist:nbins");
    const size_t Lf = f.numel();
    const size_t Lx = x.numel();
    if (Lf != Lx)
        throw Error("ecdfhist: f and x must have the same length",
                    0, 0, "ecdfhist", "", "m:ecdfhist:size");
    if (Lf < 2) {
        Value n_empty = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
        Value c_empty = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
        return {std::move(n_empty), std::move(c_empty)};
    }

    // Build (vals, probs) from the ecdf step structure.
    const size_t K = Lf - 1;
    std::vector<double> vals(K), probs(K);
    for (size_t k = 0; k < K; ++k) {
        vals[k]  = x.elemAsDouble(k + 1);
        probs[k] = f.elemAsDouble(k + 1) - f.elemAsDouble(k);
    }

    const double xmin = vals.front();
    const double xmax = vals.back();
    const double width = (xmax > xmin)
        ? (xmax - xmin) / static_cast<double>(m)
        : 1.0;  // degenerate single-value case: arbitrary width=1

    Value n_out = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
    Value c_out = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
    double *nd = n_out.doubleDataMut();
    double *cd = c_out.doubleDataMut();

    for (int k = 0; k < m; ++k) {
        cd[k] = xmin + (k + 0.5) * width;
        nd[k] = 0.0;
    }
    if (xmax <= xmin) {
        // All-equal data: drop full mass into the centre bin.
        nd[m / 2] = 1.0 / width;
        return {std::move(n_out), std::move(c_out)};
    }

    // MATLAB convention: a value v at an edge belongs to the LOWER bin,
    // i.e. bin k contains (edge[k-1], edge[k]] in 1-based indexing.
    // Equivalent: `bin = ceil((v - xmin) / width) - 1` in 0-based.
    // Bug fix 2026-05-08: previous impl used `floor`, which sent
    // boundary values to the upper bin (off-by-one shift in counts).
    for (size_t k = 0; k < K; ++k) {
        const double v = vals[k];
        int bin = static_cast<int>(std::ceil((v - xmin) / width)) - 1;
        if (bin < 0)  bin = 0;
        if (bin >= m) bin = m - 1;
        nd[bin] += probs[k];
    }
    for (int k = 0; k < m; ++k) nd[k] /= width;
    return {std::move(n_out), std::move(c_out)};
}

// ── ecdf ─────────────────────────────────────────────────────────────
// Empirical CDF / survivor / cumulative-hazard. Optional Frequency
// weights; optional 95% confidence bounds.
//
// Output shapes match MATLAB R2025b: f and x are column vectors of
// length K+1 (K = number of distinct sample values). For cdf/survivor:
//   f[0] = 0 (cdf) or 1 (survivor); x[0] = min sample value.
// For cumulative hazard: same shape but f is the Nelson-Aalen
// estimator, NOT -log(1-cdf).

// `EcdfFull` forward-declared above. f/x/flo/fup are K+1 column vectors.
EcdfFull ecdf_full(std::pmr::memory_resource *mr,
                   const Value &y,
                   const Value *freq,
                   const std::string &function_mode,
                   double alpha,
                   bool want_bounds)
{
    const size_t n = y.numel();
    const bool has_freq = (freq && freq->numel() == n);
    if (freq && freq->numel() != 0 && freq->numel() != n)
        throw Error("ecdf: Frequency length must match data length",
                    0, 0, "ecdf", "", "m:ecdf:freqsize");

    // Collect (value, weight) pairs, dropping NaNs. Sort by value.
    std::vector<std::pair<double, double>> vw;
    vw.reserve(n);
    double Wtotal = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double s = y.elemAsDouble(i);
        if (std::isnan(s)) continue;
        const double w = has_freq ? freq->elemAsDouble(i) : 1.0;
        if (w == 0.0) continue;
        vw.push_back({s, w});
        Wtotal += w;
    }
    EcdfFull R{};
    if (vw.empty() || Wtotal <= 0.0) {
        R.f   = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.x   = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.flo = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.fup = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return R;
    }
    std::sort(vw.begin(), vw.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    const double N = Wtotal;

    // Walk through sorted (v, w) and emit one row per distinct value.
    std::vector<double> fs, xs;
    std::vector<double> at_risk;   // n_i (for Nelson-Aalen and Greenwood)
    std::vector<double> events;    // d_i (events at this distinct value)
    fs.push_back(0.0);
    xs.push_back(vw.front().first);  // F = 0 at x = min(y)
    at_risk.push_back(N);
    events.push_back(0.0);

    double cum_w = 0.0;
    size_t i = 0;
    while (i < vw.size()) {
        size_t j = i + 1;
        double w_block = vw[i].second;
        while (j < vw.size() && vw[j].first == vw[i].first) {
            w_block += vw[j].second;
            ++j;
        }
        const double n_i = N - cum_w;       // at-risk just before this event
        cum_w += w_block;
        fs.push_back(cum_w / N);
        xs.push_back(vw[i].first);
        at_risk.push_back(n_i);
        events.push_back(w_block);
        i = j;
    }

    const size_t L = fs.size();

    // Apply Function mode.
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::string mode = lower(function_mode);
    std::vector<double> ff(L);
    if (mode == "cdf" || mode.empty()) {
        for (size_t k = 0; k < L; ++k) ff[k] = fs[k];
    } else if (mode == "survivor") {
        for (size_t k = 0; k < L; ++k) ff[k] = 1.0 - fs[k];
    } else if (mode == "cumulative hazard" || mode == "cumhazard") {
        // Nelson-Aalen estimator: H(x) = sum over t_i ≤ x of d_i / n_i.
        ff[0] = 0.0;
        double H = 0.0;
        for (size_t k = 1; k < L; ++k) {
            if (at_risk[k] > 0.0) H += events[k] / at_risk[k];
            ff[k] = H;
        }
    } else {
        throw Error("ecdf: unknown Function mode '" + mode + "'",
                    0, 0, "ecdf", "", "m:ecdf:badmode");
    }

    R.f = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    R.x = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    {
        double *fd = R.f.doubleDataMut();
        double *xd = R.x.doubleDataMut();
        for (size_t k = 0; k < L; ++k) { fd[k] = ff[k]; xd[k] = xs[k]; }
    }

    if (!want_bounds) {
        R.flo = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.fup = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return R;
    }

    // Greenwood-style binomial Wald CI for cdf / survivor; analogous
    // log-transform for cumulative hazard. Match MATLAB R2025b: first
    // and last rows return NaN bounds.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    R.flo = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    R.fup = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    double *lo = R.flo.doubleDataMut();
    double *hi = R.fup.doubleDataMut();

    // z = -norminv(α/2). For α=0.05 → ~1.959964.
    const double z = std::sqrt(2.0) * [&]{
        double e = 1.0 - alpha;
        for (int it = 0; it < 50; ++it) {
            const double f = std::erf(e) - (1.0 - alpha);
            const double fp = (2.0 / std::sqrt(3.14159265358979323846))
                              * std::exp(-e * e);
            e -= f / fp;
        }
        return e;
    }();
    for (size_t k = 0; k < L; ++k) {
        if (k == 0 || k == L - 1) { lo[k] = nan; hi[k] = nan; continue; }
        const double F = ff[k];
        const double se = std::sqrt(F * (1.0 - F) / N);
        double l = F - z * se;
        double h = F + z * se;
        if (l < 0.0) l = 0.0;
        if (h > 1.0) h = 1.0;
        lo[k] = l;
        hi[k] = h;
    }
    return R;
}

// Backward-compat 1-arg form.
std::tuple<Value, Value>
ecdf(std::pmr::memory_resource *mr, const Value &y)
{
    auto R = ecdf_full(mr, y, nullptr, "cdf", 0.05, false);
    return {std::move(R.f), std::move(R.x)};
}

} // namespace numkit::stats
