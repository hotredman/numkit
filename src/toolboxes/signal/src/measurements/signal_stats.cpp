// toolboxes/signal/src/measurements/signal_stats.cpp
//
// rms / rssq / peak2peak / peak2rms — signal-side reductions over a
// chosen dim. Built on the same applyAlongDim infrastructure that
// builtin stats (mean/var/std/...) use.

#include <numkit/signal/measurements/signal_stats.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/reductions.hpp>  // numkit::ops::applyAlongDim

#include <algorithm>
#include <cmath>
#include <limits>

namespace numkit::signal {

namespace {

using ::numkit::ops::applyAlongDim;
using ::numkit::ops::firstNonSingletonDim;
using ::numkit::ops::validateDim;

// Resolve user-supplied dim (0 → first non-singleton, otherwise validate).
int resolveDim(const Value &x, int dim, const char *fn)
{
    if (dim == 0)
        return firstNonSingletonDim(x);
    return validateDim(x, dim, fn);
}

} // namespace

// ── rms ────────────────────────────────────────────────────────────────
Value rms(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "rms");
    return applyAlongDim(x, d,
        [](size_t /*outIdx*/, const double *slice, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            double s = 0.0;
            for (size_t i = 0; i < n; ++i)
                s += slice[i] * slice[i];
            return std::sqrt(s / static_cast<double>(n));
        }, mr);
}

// ── rssq ───────────────────────────────────────────────────────────────
Value rssq(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "rssq");
    return applyAlongDim(x, d,
        [](size_t /*outIdx*/, const double *slice, size_t n) -> double {
            double s = 0.0;
            for (size_t i = 0; i < n; ++i)
                s += slice[i] * slice[i];
            return std::sqrt(s);
        }, mr);
}

// ── peak2peak ──────────────────────────────────────────────────────────
Value peak2peak(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "peak2peak");
    return applyAlongDim(x, d,
        [](size_t /*outIdx*/, const double *slice, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            double mn = slice[0], mx = slice[0];
            for (size_t i = 1; i < n; ++i) {
                const double v = slice[i];
                if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
                if (v < mn) mn = v;
                else if (v > mx) mx = v;
            }
            if (std::isnan(mn) || std::isnan(mx))
                return std::numeric_limits<double>::quiet_NaN();
            return mx - mn;
        }, mr);
}

// ── peak2rms ───────────────────────────────────────────────────────────
Value peak2rms(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "peak2rms");
    return applyAlongDim(x, d,
        [](size_t /*outIdx*/, const double *slice, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            double s = 0.0, peak = 0.0;
            for (size_t i = 0; i < n; ++i) {
                const double a = std::abs(slice[i]);
                s += a * a;
                if (a > peak) peak = a;
            }
            const double r = std::sqrt(s / static_cast<double>(n));
            if (r == 0.0) return std::numeric_limits<double>::infinity();
            return peak / r;
        }, mr);
}

} // namespace numkit::signal
