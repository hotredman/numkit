// libs/builtin/src/MStdStats.cpp
//
// Phase-1 stats: var, std, median, quantile, prctile, mode.
// All take an explicit `dim` argument (1-based, or 0 for "first
// non-singleton"). Implementations route through applyAlongDim from
// MStdReductionHelpers, which handles vector/2D/3D layouts uniformly.

#include <numkit/m/builtin/MStdStats.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "MStdReductionHelpers.hpp"
#include "backends/MStdNanReductions.hpp"
#include "backends/MStdVarReduction.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace numkit::m::builtin {

using detail::applyAlongDim;
using detail::applyAlongDimPair;
using detail::resolveDim;

// ────────────────────────────────────────────────────────────────────
// var / std
// ────────────────────────────────────────────────────────────────────
//
// Phase P5 + P2-followup: var / std / nanvar / nanstd all route through
// the SIMD two-pass kernels in backends/MStdVarReduction_{simd,portable}.cpp
// and MStdNanReductions_{simd,portable}.cpp. Welford's recurrence
// (numerically pristine but fully serial) is no longer used here.
namespace {

void validateNormFlag(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw MError(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("m:") + fn + ":badFlag");
}

} // namespace

MValue var(Allocator &alloc, const MValue &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "var");
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE)
        return MValue::scalar(varianceTwoPass(x.doubleData(), x.numel(), normFlag), &alloc);

    const int d = resolveDim(x, dim, "var");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return varianceTwoPass(slice, n, normFlag);
        }, &alloc);
}

MValue stdev(Allocator &alloc, const MValue &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "std");
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE)
        return MValue::scalar(std::sqrt(varianceTwoPass(x.doubleData(), x.numel(), normFlag)), &alloc);

    const int d = resolveDim(x, dim, "std");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return std::sqrt(varianceTwoPass(slice, n, normFlag));
        }, &alloc);
}

// ────────────────────────────────────────────────────────────────────
// median
// ────────────────────────────────────────────────────────────────────
//
// nth_element gives O(n) average instead of O(n log n) full sort.
// The slice is mutated in place — that's fine because the scratch
// buffer is owned by forEachSlice and reused per output index.
namespace {

double medianFromSlice(double *data, size_t n)
{
    if (n == 0) return std::nan("");
    if (n == 1) return data[0];
    const size_t mid = n / 2;
    std::nth_element(data, data + mid, data + n);
    if (n % 2 == 1)
        return data[mid];
    // Even count: average of the two middles. The "lower middle" is
    // max(data[0..mid-1]) after partial sort: nth_element guarantees
    // data[0..mid-1] all <= data[mid]; we need the largest of those.
    const double upper = data[mid];
    const double lower = *std::max_element(data, data + mid);
    return 0.5 * (lower + upper);
}

} // namespace

MValue median(Allocator &alloc, const MValue &x, int dim)
{
    const int d = resolveDim(x, dim, "median");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return medianFromSlice(slice, n);
        }, &alloc);
}

// ────────────────────────────────────────────────────────────────────
// quantile / prctile
// ────────────────────────────────────────────────────────────────────
//
// Default linear-interpolation method (MATLAB's default for quantile
// and prctile). For probability p in [0,1] and a slice of length n,
// position h = p*(n-1) (0-based). Lower index = floor(h), upper =
// ceil(h), result = lower_val + (h - floor(h)) * (upper_val - lower_val).
//
// Implementation note: when p is a vector, the slice is fully sorted
// once and then queried at each p. When p is a scalar, nth_element on
// floor(h) and ceil(h) is faster, but the difference is small for the
// typical use case.
namespace {

double quantileFromSortedSlice(const double *sorted, size_t n, double p)
{
    if (n == 0) return std::nan("");
    if (n == 1) return sorted[0];
    if (!std::isfinite(p)) return std::nan("");
    if (p <= 0.0) return sorted[0];
    if (p >= 1.0) return sorted[n - 1];
    const double h = p * static_cast<double>(n - 1);
    const size_t lo = static_cast<size_t>(std::floor(h));
    const size_t hi = static_cast<size_t>(std::ceil(h));
    const double frac = h - static_cast<double>(lo);
    return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}

// quantile/prctile share the bulk of the logic. `pScale` converts the
// raw user input to a [0,1] probability (1.0 for quantile, 1/100 for
// prctile).
MValue quantileImpl(Allocator &alloc, const MValue &x, const MValue &p,
                    int dim, double pScale, const char *fn)
{
    if (p.numel() == 0)
        throw MError(std::string(fn) + ": p must be non-empty",
                     0, 0, fn, "", std::string("m:") + fn + ":emptyP");

    // Normalize probabilities into a flat vector (so prctile sees /100)
    // then validate the post-scaling value lies in [0,1].
    std::vector<double> probs(p.numel());
    for (size_t i = 0; i < p.numel(); ++i) {
        probs[i] = p.doubleData()[i] * pScale;
        if (!(probs[i] >= 0.0 && probs[i] <= 1.0))
            throw MError(std::string(fn) + ": probabilities out of range",
                         0, 0, fn, "", std::string("m:") + fn + ":badProb");
    }

    const size_t k = probs.size();
    const int d = resolveDim(x, dim, fn);

    // Scalar p → standard one-output-per-slice path.
    if (k == 1) {
        const double pp = probs[0];
        return applyAlongDim(x, d,
            [pp](size_t, double *slice, size_t n) {
                std::sort(slice, slice + n);
                return quantileFromSortedSlice(slice, n, pp);
            }, &alloc);
    }

    // Vector p: each slice produces k outputs along the reduced dim.
    // We need to allocate the output explicitly because applyAlongDim
    // assumes 1 output per slice.
    if (x.isEmpty()) {
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    }
    if (x.dims().isVector() || x.isScalar()) {
        // Output is 1×k row vector.
        std::vector<double> sorted(x.numel());
        std::copy(x.doubleData(), x.doubleData() + x.numel(), sorted.data());
        std::sort(sorted.begin(), sorted.end());
        auto out = MValue::matrix(1, k, MType::DOUBLE, &alloc);
        for (size_t i = 0; i < k; ++i)
            out.doubleDataMut()[i] = quantileFromSortedSlice(sorted.data(),
                                                              sorted.size(),
                                                              probs[i]);
        return out;
    }

    // Build output shape: same as input but the reduced dim has size k.
    const auto &dd = x.dims();
    DimsArg outShape{dd.rows(), dd.cols(),
                     dd.is3D() ? dd.pages() : 0};
    switch (d) {
        case 1: outShape.rows  = k; break;
        case 2: outShape.cols  = k; break;
        case 3: outShape.pages = k; break;
        default: break;
    }
    auto out = createMatrix(outShape, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t outR = outShape.rows, outC = outShape.cols;
    const size_t outP = outShape.pages == 0 ? 1 : outShape.pages;
    const size_t N = detail::sliceLenForDim(x, d);
    std::vector<double> sorted(N);
    const double *src = x.doubleData();

    auto writeOut = [&](size_t rr, size_t cc, size_t pp, double v) {
        dst[pp * outR * outC + cc * outR + rr] = v;
    };

    if (d == 1) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const double *base = src + pp * R * C + c * R;
                std::copy(base, base + N, sorted.data());
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(i, c, pp,
                             quantileFromSortedSlice(sorted.data(), N, probs[i]));
            }
    } else if (d == 2) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c)
                    sorted[c] = src[pp * R * C + c * R + r];
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(r, i, pp,
                             quantileFromSortedSlice(sorted.data(), N, probs[i]));
            }
    } else if (d == 3) {
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                for (size_t pp = 0; pp < P; ++pp)
                    sorted[pp] = src[pp * R * C + c * R + r];
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 0; i < k; ++i)
                    writeOut(r, c, i,
                             quantileFromSortedSlice(sorted.data(), N, probs[i]));
            }
    }
    return out;
}

} // namespace

MValue quantile(Allocator &alloc, const MValue &x, const MValue &p, int dim)
{
    return quantileImpl(alloc, x, p, dim, 1.0, "quantile");
}

MValue prctile(Allocator &alloc, const MValue &x, const MValue &p, int dim)
{
    return quantileImpl(alloc, x, p, dim, 0.01, "prctile");
}

// ────────────────────────────────────────────────────────────────────
// mode
// ────────────────────────────────────────────────────────────────────
//
// Sort + run-length scan. For ties, MATLAB returns the smallest value —
// since we sort ascending, the first run achieving the max length is
// the smallest, so we use strict-greater comparison to keep it.
namespace {

void modeFromSlice(double *data, size_t n, double &outVal, double &outCount)
{
    if (n == 0) {
        outVal = std::nan("");
        outCount = 0.0;
        return;
    }
    std::sort(data, data + n);
    double bestVal = data[0];
    size_t bestCount = 1;
    double curVal = data[0];
    size_t curCount = 1;
    for (size_t i = 1; i < n; ++i) {
        if (data[i] == curVal) {
            ++curCount;
        } else {
            if (curCount > bestCount) {
                bestCount = curCount;
                bestVal = curVal;
            }
            curVal = data[i];
            curCount = 1;
        }
    }
    if (curCount > bestCount) {
        bestCount = curCount;
        bestVal = curVal;
    }
    outVal = bestVal;
    outCount = static_cast<double>(bestCount);
}

} // namespace

std::tuple<MValue, MValue>
mode(Allocator &alloc, const MValue &x, int dim)
{
    const int d = resolveDim(x, dim, "mode");
    auto [v, c] = applyAlongDimPair(x, d,
        [](double *slice, size_t n, double &outV, double &outC) {
            modeFromSlice(slice, n, outV, outC);
        }, &alloc);
    return std::make_tuple(std::move(v), std::move(c));
}

// ────────────────────────────────────────────────────────────────────
// NaN-aware reductions (Phase 2)
// ────────────────────────────────────────────────────────────────────
//
// All seven functions share the same skeleton:
//   1) compactNonNan(slice) — moves non-NaN to the front, returns count k
//   2) if k == 0, return the function-specific empty-slice sentinel
//      (0 for nansum, NaN for the rest — matches MATLAB semantics)
//   3) otherwise call the underlying kernel on (slice, k)
// Routing is via the same applyAlongDim path used by var/std/median etc.

using detail::compactNonNan;

// Phase P2: route nansum / nanmean through the single-pass scan kernels
// in backends/MStdNanReductions_{simd,portable}.cpp. These read the input
// once with an IsNaN mask and skip the compactNonNan scratch copy that
// the older scalar lambda needed. Vector input bypasses applyAlongDim
// entirely (no per-call scratch alloc); matrix dim slices still go
// through applyAlongDim but the lambda no longer mutates the slice.

MValue nansum(Allocator &alloc, const MValue &x, int dim)
{
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE)
        return MValue::scalar(nanSumScan(x.doubleData(), x.numel()), &alloc);

    const int d = resolveDim(x, dim, "nansum");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanSumScan(slice, n); // all-NaN → 0
        }, &alloc);
}

MValue nanmean(Allocator &alloc, const MValue &x, int dim)
{
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE) {
        const auto r = nanSumCountScan(x.doubleData(), x.numel());
        return MValue::scalar(r.count > 0 ? r.sum / static_cast<double>(r.count)
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

// Phase P2-followup: nanmax / nanmin / nanvar / nanstd now use the
// single-pass SIMD scans in MStdNanReductions_{simd,portable}.cpp. Same
// pattern as nansum/nanmean: vector input bypasses applyAlongDim
// entirely; matrix dim slices keep the helper. compactNonNan is no
// longer needed for these — the kernels mask NaN lanes inline.

MValue nanmax(Allocator &alloc, const MValue &x, int dim)
{
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE)
        return MValue::scalar(nanMaxScan(x.doubleData(), x.numel()), &alloc);

    const int d = resolveDim(x, dim, "nanmax");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanMaxScan(slice, n);
        }, &alloc);
}

MValue nanmin(Allocator &alloc, const MValue &x, int dim)
{
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE)
        return MValue::scalar(nanMinScan(x.doubleData(), x.numel()), &alloc);

    const int d = resolveDim(x, dim, "nanmin");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            return nanMinScan(slice, n);
        }, &alloc);
}

MValue nanvar(Allocator &alloc, const MValue &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "nanvar");
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE)
        return MValue::scalar(nanVarianceTwoPass(x.doubleData(), x.numel(), normFlag), &alloc);

    const int d = resolveDim(x, dim, "nanvar");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return nanVarianceTwoPass(slice, n, normFlag);
        }, &alloc);
}

MValue nanstdev(Allocator &alloc, const MValue &x, int normFlag, int dim)
{
    validateNormFlag(normFlag, "nanstd");
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if ((x.dims().isVector() || x.isScalar()) && x.type() == MType::DOUBLE)
        return MValue::scalar(std::sqrt(nanVarianceTwoPass(x.doubleData(), x.numel(), normFlag)), &alloc);

    const int d = resolveDim(x, dim, "nanstd");
    return applyAlongDim(x, d,
        [normFlag](size_t, double *slice, size_t n) {
            return std::sqrt(nanVarianceTwoPass(slice, n, normFlag));
        }, &alloc);
}

MValue nanmedian(Allocator &alloc, const MValue &x, int dim)
{
    const int d = resolveDim(x, dim, "nanmedian");
    return applyAlongDim(x, d,
        [](size_t, double *slice, size_t n) {
            const size_t k = compactNonNan(slice, n);
            return medianFromSlice(slice, k); // returns NaN at k==0
        }, &alloc);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

void var_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw MError("var: requires at least 1 argument",
                     0, 0, "var", "", "m:var:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = var(ctx.engine->allocator(), args[0], w, dim);
}

void std_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw MError("std: requires at least 1 argument",
                     0, 0, "std", "", "m:std:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = stdev(ctx.engine->allocator(), args[0], w, dim);
}

void median_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw MError("median: requires at least 1 argument",
                     0, 0, "median", "", "m:median:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    outs[0] = median(ctx.engine->allocator(), args[0], dim);
}

void quantile_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("quantile: requires (X, p[, dim])",
                     0, 0, "quantile", "", "m:quantile:nargin");
    int dim = 0;
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = quantile(ctx.engine->allocator(), args[0], args[1], dim);
}

void prctile_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("prctile: requires (X, p[, dim])",
                     0, 0, "prctile", "", "m:prctile:nargin");
    int dim = 0;
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = prctile(ctx.engine->allocator(), args[0], args[1], dim);
}

void mode_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw MError("mode: requires at least 1 argument",
                     0, 0, "mode", "", "m:mode:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    auto [v, c] = mode(ctx.engine->allocator(), args[0], dim);
    outs[0] = std::move(v);
    if (nargout > 1)
        outs[1] = std::move(c);
}

// nan* adapters — all accept (X) or (X, dim), except nanvar/nanstd
// which additionally take (X, w) / (X, w, dim).

#define NK_NAN_REDUCTION_ADAPTER(name, fn)                                      \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,                \
                    Span<MValue> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw MError(#name ": requires at least 1 argument",                 \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        int dim = 0;                                                             \
        if (args.size() >= 2 && !args[1].isEmpty())                              \
            dim = static_cast<int>(args[1].toScalar());                          \
        outs[0] = fn(ctx.engine->allocator(), args[0], dim);                     \
    }

NK_NAN_REDUCTION_ADAPTER(nansum,    nansum)
NK_NAN_REDUCTION_ADAPTER(nanmean,   nanmean)
NK_NAN_REDUCTION_ADAPTER(nanmax,    nanmax)
NK_NAN_REDUCTION_ADAPTER(nanmin,    nanmin)
NK_NAN_REDUCTION_ADAPTER(nanmedian, nanmedian)

#undef NK_NAN_REDUCTION_ADAPTER

void nanvar_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw MError("nanvar: requires at least 1 argument",
                     0, 0, "nanvar", "", "m:nanvar:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanvar(ctx.engine->allocator(), args[0], w, dim);
}

void nanstd_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw MError("nanstd: requires at least 1 argument",
                     0, 0, "nanstd", "", "m:nanstd:nargin");
    int w = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        w = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = nanstdev(ctx.engine->allocator(), args[0], w, dim);
}

} // namespace detail

} // namespace numkit::m::builtin
