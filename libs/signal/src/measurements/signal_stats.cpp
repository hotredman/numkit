// libs/signal/src/measurements/signal_stats.cpp
//
// rms / rssq / peak2peak / peak2rms — signal-side reductions over a
// chosen dim. Built on the same applyAlongDim infrastructure that
// builtin stats (mean/var/std/...) use.

#include <numkit/signal/measurements/signal_stats.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "reduction_helpers.hpp"  // numkit::builtin::detail::applyAlongDim

#include <algorithm>
#include <cmath>
#include <limits>

namespace numkit::signal {

namespace {

using ::numkit::builtin::detail::applyAlongDim;
using ::numkit::builtin::detail::firstNonSingletonDim;
using ::numkit::builtin::detail::validateDim;

// Resolve user-supplied dim (0 → first non-singleton, otherwise validate).
int resolveDim(const Value &x, int dim, const char *fn)
{
    if (dim == 0)
        return firstNonSingletonDim(x);
    return validateDim(x, dim, fn);
}

} // namespace

// ── rms ────────────────────────────────────────────────────────────────
Value rms(std::pmr::memory_resource *mr, const Value &x, int dim)
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
Value rssq(std::pmr::memory_resource *mr, const Value &x, int dim)
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
Value peak2peak(std::pmr::memory_resource *mr, const Value &x, int dim)
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
Value peak2rms(std::pmr::memory_resource *mr, const Value &x, int dim)
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

namespace detail {

static int dimFromArg(Span<const Value> args)
{
    return (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
}

void rms_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rms: requires at least 1 argument",
                     0, 0, "rms", "", "m:rms:nargin");
    outs[0] = rms(ctx.engine->resource(), args[0], dimFromArg(args));
}

void rssq_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rssq: requires at least 1 argument",
                     0, 0, "rssq", "", "m:rssq:nargin");
    outs[0] = rssq(ctx.engine->resource(), args[0], dimFromArg(args));
}

void peak2peak_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("peak2peak: requires at least 1 argument",
                     0, 0, "peak2peak", "", "m:peak2peak:nargin");
    outs[0] = peak2peak(ctx.engine->resource(), args[0], dimFromArg(args));
}

void peak2rms_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("peak2rms: requires at least 1 argument",
                     0, 0, "peak2rms", "", "m:peak2rms:nargin");
    outs[0] = peak2rms(ctx.engine->resource(), args[0], dimFromArg(args));
}

} // namespace detail

} // namespace numkit::signal
