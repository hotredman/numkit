// libs/stats/src/nan_aware/nan_aware.cpp
//
// nansum / nanmean / nanmax / nanmin / nanvar / nanstdev / nanmedian.
// Extracted from libs/builtin/src/stats.cpp during the
// MATLAB-taxonomy refactor — Statistics Toolbox content.

#include <numkit/stats/nan_aware/nan_aware.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "backends/nan_reductions.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::stats {

using ::numkit::builtin::detail::applyAlongDim;
using ::numkit::builtin::detail::resolveDim;
using ::numkit::stats::detail::nanSumScan;
using ::numkit::stats::detail::nanSumCountScan;
using ::numkit::stats::detail::nanMaxScan;
using ::numkit::stats::detail::nanMinScan;
using ::numkit::stats::detail::nanVarianceTwoPass;
using ::numkit::builtin::detail::compactNonNan;

namespace {

void validateNormFlag(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("numkit:") + fn + ":badFlag");
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

namespace {

// Read x as a column-major n×p matrix; vector → n×1.
void readObsMatrix(const Value &x, ScratchVec<double> &out,
                    std::size_t &n, std::size_t &p)
{
    if (x.isEmpty()) { n = p = 0; return; }
    if (x.dims().isVector()) {
        n = x.numel();
        p = 1;
    } else {
        n = x.dims().rows();
        p = x.dims().cols();
    }
    out.resize(n * p);
    if (x.type() == ValueType::DOUBLE) {
        // Column-major copy — same layout as Value storage.
        const double *src = x.doubleData();
        std::memcpy(out.data(), src, n * p * sizeof(double));
    } else {
        for (std::size_t k = 0; k < n * p; ++k)
            out[k] = x.elemAsDouble(k);
    }
}

// Drop rows where any column has NaN. Compacts in place. Returns the
// new row count.
std::size_t dropNanRows(double *data, std::size_t n, std::size_t p)
{
    std::size_t w = 0;
    for (std::size_t r = 0; r < n; ++r) {
        bool keep = true;
        for (std::size_t c = 0; c < p; ++c) {
            if (std::isnan(data[c * n + r])) { keep = false; break; }
        }
        if (keep) {
            // Copy row r to row w (column by column).
            if (w != r)
                for (std::size_t c = 0; c < p; ++c)
                    data[c * n + w] = data[c * n + r];
            ++w;
        }
    }
    // Tail rows [w..n) on each column are now garbage — but the kept
    // count is what we use downstream. Caller treats `w` as new n.
    return w;
}

// In-place column centering for an n_kept × p block stored with stride
// n_orig (NOT compacted). To keep things simple we instead PACK the
// kept rows into a fresh contiguous n_kept × p layout before centering.
void packKeptRows(const double *src, std::size_t n_orig,
                   std::size_t n_kept, std::size_t p, double *dst)
{
    for (std::size_t c = 0; c < p; ++c)
        for (std::size_t r = 0; r < n_kept; ++r)
            dst[c * n_kept + r] = src[c * n_orig + r];
}

void centerColumnsLocal(double *data, std::size_t n, std::size_t p)
{
    for (std::size_t c = 0; c < p; ++c) {
        double s = 0.0;
        for (std::size_t r = 0; r < n; ++r) s += data[c * n + r];
        const double m = s / static_cast<double>(n);
        for (std::size_t r = 0; r < n; ++r) data[c * n + r] -= m;
    }
}

Value covFromCenteredLocal(const double *X, std::size_t n,
                            std::size_t p, double divisor,
                            std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(p, p, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            double s = 0.0;
            for (std::size_t r = 0; r < n; ++r)
                s += X[i * n + r] * X[j * n + r];
            dst[j * p + i] = s / divisor;
        }
    return out;
}

} // namespace

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

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

#define NK_NAN_REDUCTION_ADAPTER(name, fn)                                      \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        int dim = 0;                                                             \
        if (args.size() >= 2 && !args[1].isEmpty())                              \
            dim = static_cast<int>(args[1].toScalar());                          \
        outs[0] = fn(args[0], dim, ctx.engine->resource());                     \
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
                         0, 0, #name, "", "numkit:" #name ":nargin");                  \
        int dim = 0;                                                              \
        if (args.size() == 2 && !args[1].isEmpty())                               \
            dim = static_cast<int>(args[1].toScalar());                           \
        else if (args.size() >= 3 && !args[2].isEmpty())                          \
            dim = static_cast<int>(args[2].toScalar());                           \
        outs[0] = fn(args[0], dim, ctx.engine->resource());                      \
    }

NK_NAN_MAXMIN_ADAPTER(nanmax, nanmax)
NK_NAN_MAXMIN_ADAPTER(nanmin, nanmin)

#undef NK_NAN_MAXMIN_ADAPTER

void nanvar_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nanvar: requires at least 1 argument",
                     0, 0, "nanvar", "", "numkit:nanvar:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanvar(args[0], w, dim, ctx.engine->resource());
}

void nanstd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nanstd: requires at least 1 argument",
                     0, 0, "nanstd", "", "numkit:nanstd:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanstdev(args[0], w, dim, ctx.engine->resource());
}

// nancov has two MATLAB call patterns:
//   nancov(X)                   — covariance matrix of X
//   nancov(X, normFlag)         — normalization 0 (n-1) or 1 (n)
//   nancov(X, Y)                — between two vectors → 2×2
//   nancov(X, Y, normFlag)      — same + normalization
// We disambiguate by arg-2 type: scalar → normFlag; non-scalar → Y.
void nancov_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("nancov: requires at least 1 argument",
                     0, 0, "nancov", "", "numkit:nancov:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = nancov(args[0], 0, mr);
        return;
    }
    // args.size() >= 2
    const bool secondIsScalar = (args[1].numel() == 1) && !args[1].isEmpty();
    if (secondIsScalar) {
        const int w = static_cast<int>(args[1].toScalar());
        outs[0] = nancov(args[0], w, mr);
    } else {
        int w = 0;
        if (args.size() >= 3 && !args[2].isEmpty())
            w = static_cast<int>(args[2].toScalar());
        outs[0] = nancov(args[0], args[1], w, mr);
    }
}

} // namespace detail

} // namespace numkit::stats
