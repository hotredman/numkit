// libs/stats/src/nan_aware/nan_aware.cpp
//
// nansum / nanmean / nanmax / nanmin / nanvar / nanstdev / nanmedian.
// Extracted from libs/builtin/src/MStdStats.cpp during the
// MATLAB-taxonomy refactor — Statistics Toolbox content.

#include <numkit/stats/nan_aware/nan_aware.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "backends/nan_reductions.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace numkit::stats {

using ::numkit::builtin::detail::applyAlongDim;
using ::numkit::builtin::detail::resolveDim;
using ::numkit::builtin::nanSumScan;
using ::numkit::builtin::nanSumCountScan;
using ::numkit::builtin::nanMaxScan;
using ::numkit::builtin::nanMinScan;
using ::numkit::builtin::nanVarianceTwoPass;
using ::numkit::builtin::detail::compactNonNan;

namespace {

void validateNormFlag(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("m:") + fn + ":badFlag");
}

double medianFromSlice(double *data, size_t n)
{
    if (n == 0) return std::nan("");
    if (n == 1) return data[0];
    const size_t mid = n / 2;
    std::nth_element(data, data + mid, data + n);
    if (n % 2 == 1)
        return data[mid];
    const double upper = data[mid];
    const double lower = *std::max_element(data, data + mid);
    return 0.5 * (lower + upper);
}

} // namespace

Value nansum(Allocator &alloc, const Value &x, int dim)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanSumScan(x.doubleData(), x.numel()), &alloc);

    const int d = resolveDim(x, dim, "nansum");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanSumScan(slice, n); // all-NaN → 0
        }, &alloc);
}

Value nanmean(Allocator &alloc, const Value &x, int dim)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE) {
        const auto r = nanSumCountScan(x.doubleData(), x.numel());
        return Value::scalar(r.count > 0 ? r.sum / static_cast<double>(r.count)
                                          : std::nan(""), &alloc);
    }

    const int d = resolveDim(x, dim, "nanmean");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            const auto r = nanSumCountScan(slice, n);
            return r.count > 0 ? r.sum / static_cast<double>(r.count)
                               : std::nan("");
        }, &alloc);
}

Value nanmax(Allocator &alloc, const Value &x, int dim)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanMaxScan(x.doubleData(), x.numel()), &alloc);

    const int d = resolveDim(x, dim, "nanmax");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanMaxScan(slice, n);
        }, &alloc);
}

Value nanmin(Allocator &alloc, const Value &x, int dim)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanMinScan(x.doubleData(), x.numel()), &alloc);

    const int d = resolveDim(x, dim, "nanmin");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanMinScan(slice, n);
        }, &alloc);
}

Value nanvar(Allocator &alloc, const Value &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "nanvar");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(nanVarianceTwoPass(x.doubleData(), x.numel(), normFlag), &alloc);

    const int d = resolveDim(x, dim, "nanvar");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return nanVarianceTwoPass(slice, n, normFlag);
        }, &alloc);
}

Value nanstdev(Allocator &alloc, const Value &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "nanstd");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == ValueType::DOUBLE)
        return Value::scalar(std::sqrt(nanVarianceTwoPass(x.doubleData(), x.numel(), normFlag)), &alloc);

    const int d = resolveDim(x, dim, "nanstd");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return std::sqrt(nanVarianceTwoPass(slice, n, normFlag));
        }, &alloc);
}

Value nanmedian(Allocator &alloc, const Value &x, int dim)
{
    const int d = resolveDim(x, dim, "nanmedian");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            const size_t k = compactNonNan(slice, n);
            return medianFromSlice(slice, k); // returns NaN at k==0
        }, &alloc);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

#define NK_NAN_REDUCTION_ADAPTER(name, fn)                                      \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        int dim = 0;                                                             \
        if (args.size() >= 2 && !args[1].isEmpty())                              \
            dim = static_cast<int>(args[1].toScalar());                          \
        outs[0] = fn(ctx.engine->allocator(), args[0], dim);                     \
    }

NK_NAN_REDUCTION_ADAPTER(nansum,    nansum)
NK_NAN_REDUCTION_ADAPTER(nanmean,   nanmean)
NK_NAN_REDUCTION_ADAPTER(nanmedian, nanmedian)

#undef NK_NAN_REDUCTION_ADAPTER

// nanmax / nanmin accept both signatures:
//   nanmax(A)         — reduce over first non-singleton
//   nanmax(A, dim)    — legacy/numkit form (dim in arg 1)
//   nanmax(A, [], d)  — MATLAB-style 3-arg form (dim in arg 2; arg 1 = [])
#define NK_NAN_MAXMIN_ADAPTER(name, fn)                                          \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                 \
                    Span<Value> outs, CallContext &ctx)                         \
    {                                                                             \
        if (args.empty())                                                         \
            throw Error(#name ": requires at least 1 argument",                  \
                         0, 0, #name, "", "m:" #name ":nargin");                  \
        int dim = 0;                                                              \
        if (args.size() == 2 && !args[1].isEmpty())                               \
            dim = static_cast<int>(args[1].toScalar());                           \
        else if (args.size() >= 3 && !args[2].isEmpty())                          \
            dim = static_cast<int>(args[2].toScalar());                           \
        outs[0] = fn(ctx.engine->allocator(), args[0], dim);                      \
    }

NK_NAN_MAXMIN_ADAPTER(nanmax, nanmax)
NK_NAN_MAXMIN_ADAPTER(nanmin, nanmin)

#undef NK_NAN_MAXMIN_ADAPTER

void nanvar_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nanvar: requires at least 1 argument",
                     0, 0, "nanvar", "", "m:nanvar:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanvar(ctx.engine->allocator(), args[0], w, dim);
}

void nanstd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nanstd: requires at least 1 argument",
                     0, 0, "nanstd", "", "m:nanstd:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanstdev(ctx.engine->allocator(), args[0], w, dim);
}

} // namespace detail

} // namespace numkit::stats
