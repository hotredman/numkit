// libs/signal/src/measurements/sigroi.cpp
//
// Signal Processing Toolbox region-of-interest (ROI) utilities. All 8
// functions live in one TU since they share the same data shape:
// 2-column [start, end] matrix of 1-based ROI indices.
//
//   binmask2sigroi(m)              binary mask  → ROI [start end] pairs
//   sigroi2binmask(roi [, len])    ROIs → binary mask (column vector)
//   extendsigroi(roi, Lpre, Lpost) extend each ROI in both directions
//   shortensigroi(roi, Lpre, Lpost) opposite (drop ROIs that collapse)
//   mergesigroi(roi, sep)          merge ROIs with gap ≤ sep
//   removesigroi(roi, idx)         drop ROIs at idx (1-based)
//   extractsigroi(x, roi [, concat])  extract signal slices
//                                  (cell array by default; concatenated
//                                  vector when concat=true)
//   sigrangebinmask(x, vmin, vmax) mask where vmin ≤ x ≤ vmax
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr. Scratch
// via ScratchArena/ScratchVec.

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace numkit::signal {

namespace {

inline bool truthyAt(const Value &m, size_t i)
{
    if (m.isLogical()) return m.logicalData()[i] != 0;
    return m.elemAsDouble(i) != 0.0;
}

inline int64_t roiStart(const Value &roi, size_t i)
{
    return static_cast<int64_t>(roi.elemAsDouble(i + 0 * roi.dims().rows()));
}
inline int64_t roiEnd(const Value &roi, size_t i)
{
    return static_cast<int64_t>(roi.elemAsDouble(i + 1 * roi.dims().rows()));
}

} // namespace

// ── binmask2sigroi ────────────────────────────────────────────────────
// Find runs of true values in mask m. Returns N×2 [start, end] matrix
// (1-based, inclusive).
Value binmask2sigroi(const Value &m, std::pmr::memory_resource *mr)
{
    const size_t L = m.numel();
    ScratchArena scratch(mr);
    ScratchVec<int64_t> starts(0, &scratch), ends(0, &scratch);
    bool inRun = false;
    int64_t curStart = 0;
    for (size_t i = 0; i < L; ++i) {
        const bool v = truthyAt(m, i);
        if (v && !inRun) { inRun = true; curStart = static_cast<int64_t>(i + 1); }
        else if (!v && inRun) {
            inRun = false;
            starts.push_back(curStart);
            ends.push_back(static_cast<int64_t>(i));  // i = position past last
        }
    }
    if (inRun) {
        starts.push_back(curStart);
        ends.push_back(static_cast<int64_t>(L));
    }
    const size_t N = starts.size();
    Value out = Value::matrix(N, N == 0 ? 0 : 2, ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        od[i + 0 * N] = static_cast<double>(starts[i]);
        od[i + 1 * N] = static_cast<double>(ends[i]);
    }
    return out;
}

// ── sigroi2binmask ────────────────────────────────────────────────────
// Build a logical column vector of length `len` (auto = max end) with
// true on each [start..end] segment.
Value sigroi2binmask(const Value &roi, int64_t len_user,
                     std::pmr::memory_resource *mr)
{
    const size_t N = roi.dims().rows();
    int64_t len = len_user;
    if (len < 0) {
        len = 0;
        for (size_t i = 0; i < N; ++i)
            len = std::max(len, roiEnd(roi, i));
    }
    Value out = Value::matrix(static_cast<size_t>(len), len == 0 ? 0 : 1,
                              ValueType::LOGICAL, mr);
    if (len == 0) return out;
    uint8_t *od = out.logicalDataMut();
    std::fill(od, od + len, uint8_t(0));
    for (size_t i = 0; i < N; ++i) {
        const int64_t s = std::max<int64_t>(1, roiStart(roi, i));
        const int64_t e = std::min<int64_t>(len, roiEnd(roi, i));
        for (int64_t k = s; k <= e; ++k) od[k - 1] = 1;
    }
    return out;
}

// ── extendsigroi ──────────────────────────────────────────────────────
// Extend each ROI by Lpre on the left and Lpost on the right (clamping
// start to ≥ 1). End is unbounded (no signal length provided).
Value extendsigroi(const Value &roi, int64_t Lpre, int64_t Lpost,
                   std::pmr::memory_resource *mr)
{
    const size_t N = roi.dims().rows();
    Value out = Value::matrix(N, N == 0 ? 0 : 2, ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        int64_t s = roiStart(roi, i) - Lpre;
        int64_t e = roiEnd(roi, i)   + Lpost;
        if (s < 1) s = 1;
        od[i + 0 * N] = static_cast<double>(s);
        od[i + 1 * N] = static_cast<double>(e);
    }
    return out;
}

// ── shortensigroi ─────────────────────────────────────────────────────
// Inverse of extend: shrink each ROI by Lpre on the left, Lpost on right.
// Drop ROIs that collapse (start > end).
Value shortensigroi(const Value &roi, int64_t Lpre, int64_t Lpost,
                    std::pmr::memory_resource *mr)
{
    const size_t N = roi.dims().rows();
    ScratchArena scratch(mr);
    ScratchVec<int64_t> starts(0, &scratch), ends(0, &scratch);
    for (size_t i = 0; i < N; ++i) {
        int64_t s = roiStart(roi, i) + Lpre;
        int64_t e = roiEnd(roi, i)   - Lpost;
        if (s <= e) { starts.push_back(s); ends.push_back(e); }
    }
    const size_t M = starts.size();
    Value out = Value::matrix(M, M == 0 ? 0 : 2, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < M; ++i) {
        od[i + 0 * M] = static_cast<double>(starts[i]);
        od[i + 1 * M] = static_cast<double>(ends[i]);
    }
    return out;
}

// ── mergesigroi ───────────────────────────────────────────────────────
// Sort ROIs by start, merge those with gap ≤ sep.
//   sep == 0 → only overlapping ROIs merge
//   sep ≥ 1 → adjacent or near-adjacent merge
Value mergesigroi(const Value &roi, int64_t sep, std::pmr::memory_resource *mr)
{
    const size_t N = roi.dims().rows();
    ScratchArena scratch(mr);
    ScratchVec<size_t> idx(N, &scratch);
    for (size_t i = 0; i < N; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(),
              [&](size_t a, size_t b) { return roiStart(roi, a) < roiStart(roi, b); });

    ScratchVec<int64_t> starts(0, &scratch), ends(0, &scratch);
    for (size_t k = 0; k < N; ++k) {
        const int64_t s = roiStart(roi, idx[k]);
        const int64_t e = roiEnd(roi, idx[k]);
        if (!starts.empty() && s - ends.back() <= sep + 1) {
            // Merge: extend the previous end if e is larger.
            if (e > ends.back()) ends.back() = e;
        } else {
            starts.push_back(s);
            ends.push_back(e);
        }
    }
    const size_t M = starts.size();
    Value out = Value::matrix(M, M == 0 ? 0 : 2, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < M; ++i) {
        od[i + 0 * M] = static_cast<double>(starts[i]);
        od[i + 1 * M] = static_cast<double>(ends[i]);
    }
    return out;
}

// ── removesigroi ──────────────────────────────────────────────────────
// Drop ROIs whose length (end - start + 1) is ≤ maxLen. Per MATLAB doc:
// "removes signal regions of interest specified in roilims that have a
// length of s samples or less."
Value removesigroi(const Value &roi, int64_t maxLen,
                   std::pmr::memory_resource *mr)
{
    const size_t N = roi.dims().rows();
    ScratchArena scratch(mr);
    ScratchVec<int64_t> starts(0, &scratch), ends(0, &scratch);
    for (size_t i = 0; i < N; ++i) {
        const int64_t s = roiStart(roi, i);
        const int64_t e = roiEnd(roi, i);
        const int64_t len = e - s + 1;
        if (len > maxLen) {
            starts.push_back(s);
            ends.push_back(e);
        }
    }
    const size_t M = starts.size();
    Value out = Value::matrix(M, M == 0 ? 0 : 2, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < M; ++i) {
        od[i + 0 * M] = static_cast<double>(starts[i]);
        od[i + 1 * M] = static_cast<double>(ends[i]);
    }
    return out;
}

// ── extractsigroi ─────────────────────────────────────────────────────
// Default: cell array (one cell per ROI, each a column vector slice).
// concat=true: concatenate all slices into a single column vector.
Value extractsigroi(const Value &x, const Value &roi, bool concat,
                    std::pmr::memory_resource *mr)
{
    const size_t N = roi.dims().rows();
    const int64_t L = static_cast<int64_t>(x.numel());
    if (concat) {
        // Compute total length first.
        int64_t totalLen = 0;
        for (size_t i = 0; i < N; ++i) {
            const int64_t s = std::max<int64_t>(1, roiStart(roi, i));
            const int64_t e = std::min<int64_t>(L, roiEnd(roi, i));
            if (e >= s) totalLen += (e - s + 1);
        }
        Value out = Value::matrix(static_cast<size_t>(totalLen),
                                  totalLen == 0 ? 0 : 1,
                                  ValueType::DOUBLE, mr);
        if (totalLen == 0) return out;
        double *od = out.doubleDataMut();
        size_t pos = 0;
        for (size_t i = 0; i < N; ++i) {
            const int64_t s = std::max<int64_t>(1, roiStart(roi, i));
            const int64_t e = std::min<int64_t>(L, roiEnd(roi, i));
            for (int64_t k = s; k <= e; ++k)
                od[pos++] = x.elemAsDouble(static_cast<size_t>(k - 1));
        }
        return out;
    }
    // Cell-array form.
    Value out = Value::cell(N, N == 0 ? 0 : 1, mr);
    for (size_t i = 0; i < N; ++i) {
        const int64_t s = std::max<int64_t>(1, roiStart(roi, i));
        const int64_t e = std::min<int64_t>(L, roiEnd(roi, i));
        const int64_t segLen = (e >= s) ? (e - s + 1) : 0;
        Value seg = Value::matrix(static_cast<size_t>(segLen),
                                  segLen == 0 ? 0 : 1,
                                  ValueType::DOUBLE, mr);
        if (segLen > 0) {
            double *sd = seg.doubleDataMut();
            for (int64_t k = 0; k < segLen; ++k)
                sd[k] = x.elemAsDouble(static_cast<size_t>(s - 1 + k));
        }
        out.cellAt(i) = std::move(seg);
    }
    return out;
}

// ── sigrangebinmask ───────────────────────────────────────────────────
// MATLAB signature: sigrangebinmask(x, bound).
//   bound is scalar  → mask where x > bound  (default Relationship='above')
//   bound is 2-vec   → mask where bound(1) <= x <= bound(2)
//                      (default Relationship='inside', closed interval)
// 'Relationship' / 'IntervalType' name-value args deferred (KNOWN GAP).
Value sigrangebinmask(const Value &x, const Value &bound,
                      std::pmr::memory_resource *mr)
{
    const size_t L = x.numel();
    Value out;
    if (x.dims().rows() == 1)
        out = Value::matrix(1, L, ValueType::LOGICAL, mr);
    else
        out = Value::matrix(L, L == 0 ? 0 : 1, ValueType::LOGICAL, mr);
    if (L == 0) return out;
    uint8_t *od = out.logicalDataMut();
    if (bound.numel() == 1) {
        const double b = bound.toScalar();
        for (size_t i = 0; i < L; ++i)
            od[i] = (x.elemAsDouble(i) > b) ? 1 : 0;
    } else if (bound.numel() == 2) {
        const double vmin = bound.elemAsDouble(0);
        const double vmax = bound.elemAsDouble(1);
        for (size_t i = 0; i < L; ++i) {
            const double v = x.elemAsDouble(i);
            od[i] = (v >= vmin && v <= vmax) ? 1 : 0;
        }
    } else {
        throw Error("sigrangebinmask: bound must be scalar or 2-element vector",
                    0, 0, "sigrangebinmask", "", "m:sigrangebinmask:BadBound");
    }
    return out;
}

namespace detail {

void binmask2sigroi_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("binmask2sigroi: requires (mask)",
                    0, 0, "binmask2sigroi", "", "m:binmask2sigroi:nargin");
    outs[0] = binmask2sigroi(args[0], ctx.engine->resource());
}

void sigroi2binmask_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sigroi2binmask: requires (roi [, len])",
                    0, 0, "sigroi2binmask", "", "m:sigroi2binmask:nargin");
    int64_t len = -1;
    if (args.size() >= 2) len = static_cast<int64_t>(args[1].toScalar());
    outs[0] = sigroi2binmask(args[0], len, ctx.engine->resource());
}

void extendsigroi_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("extendsigroi: requires (roi, Lpre, Lpost)",
                    0, 0, "extendsigroi", "", "m:extendsigroi:nargin");
    outs[0] = extendsigroi(args[0],
                           static_cast<int64_t>(args[1].toScalar()),
                           static_cast<int64_t>(args[2].toScalar()),
                           ctx.engine->resource());
}

void shortensigroi_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("shortensigroi: requires (roi, Lpre, Lpost)",
                    0, 0, "shortensigroi", "", "m:shortensigroi:nargin");
    outs[0] = shortensigroi(args[0],
                            static_cast<int64_t>(args[1].toScalar()),
                            static_cast<int64_t>(args[2].toScalar()),
                            ctx.engine->resource());
}

void mergesigroi_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mergesigroi: requires (roi, sep)",
                    0, 0, "mergesigroi", "", "m:mergesigroi:nargin");
    outs[0] = mergesigroi(args[0],
                          static_cast<int64_t>(args[1].toScalar()),
                          ctx.engine->resource());
}

void removesigroi_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("removesigroi: requires (roi, maxLen)",
                    0, 0, "removesigroi", "", "m:removesigroi:nargin");
    outs[0] = removesigroi(args[0],
                           static_cast<int64_t>(args[1].toScalar()),
                           ctx.engine->resource());
}

void extractsigroi_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("extractsigroi: requires (x, roi [, concat])",
                    0, 0, "extractsigroi", "", "m:extractsigroi:nargin");
    bool concat = false;
    if (args.size() >= 3) concat = (args[2].toScalar() != 0.0);
    outs[0] = extractsigroi(args[0], args[1], concat, ctx.engine->resource());
}

void sigrangebinmask_reg(Span<const Value> args, size_t /*nargout*/,
                         Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sigrangebinmask: requires (x, bound) where bound is "
                    "scalar (above) or 2-vec [vmin vmax] (inside)",
                    0, 0, "sigrangebinmask", "", "m:sigrangebinmask:nargin");
    outs[0] = sigrangebinmask(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
