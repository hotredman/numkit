// libs/builtin/src/math/group/group.cpp
//
// Group-based operations:
//   findgroups   — assign 1-based group IDs to each element
//   splitapply   — apply function per group
//   groupcounts  — count elements per group

#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

namespace numkit::builtin {
namespace detail {

namespace {

// Build a sorted-unique value list with a parallel "group id per
// element" vector (1-based). Operates on numeric input via
// elemAsDouble (CHAR / LOGICAL fall through naturally).
//
// out_groups[i] is the group id of g[i]; out_unique is the sorted
// unique value list (length = max(out_groups)). NaN entries get
// out_groups[i] = 0 (sentinel for "missing"). The caller decides
// what to do with them (findgroups → NaN; groupcounts → trailing
// NaN bucket).
void groupOf(const Value &g, std::vector<std::size_t> &out_groups,
             std::vector<double> &out_unique)
{
    const std::size_t n = g.numel();
    out_groups.assign(n, 0);
    if (n == 0) { out_unique.clear(); return; }

    std::vector<double> vals(n);
    for (std::size_t i = 0; i < n; ++i) vals[i] = g.elemAsDouble(i);

    // Build sorted-unique list of finite values (skip NaN — handled
    // separately by callers).
    std::vector<double> sorted;
    sorted.reserve(n);
    for (double v : vals)
        if (!std::isnan(v)) sorted.push_back(v);
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end(),
                              [](double a, double b) {
                                  return a == b;
                              }), sorted.end());
    out_unique = sorted;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(vals[i])) { out_groups[i] = 0; continue; }
        const auto it = std::lower_bound(sorted.begin(), sorted.end(), vals[i]);
        out_groups[i] = (std::size_t)(it - sorted.begin()) + 1;   // 1-based
    }
}

} // namespace

// ── findgroups ───────────────────────────────────────────────────────
// [G, ID] = findgroups(g) — G[i] is the group ID of g[i] (1-based,
// based on sorted-unique order); ID is the column vector of unique
// non-NaN values. NaN entries in g map to G[i]=NaN (matches MATLAB
// R2025b: NaN treated as missing, not a group).
void findgroups_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("findgroups: requires 1 argument",
                     0, 0, "findgroups", "", "m:findgroups:nargin");
    auto *mr = ctx.engine->resource();
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(args[0], groups, uniqueVals);
    auto G = Value::matrix(args[0].dims().rows(), args[0].dims().cols(),
                           ValueType::DOUBLE, mr);
    double *gd = G.doubleDataMut();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t i = 0; i < groups.size(); ++i)
        gd[i] = (groups[i] == 0) ? nan : double(groups[i]);
    outs[0] = std::move(G);
    if (nargout >= 2) {
        auto ID = Value::matrix(uniqueVals.size(), 1, ValueType::DOUBLE, mr);
        if (!uniqueVals.empty())
            std::memcpy(ID.doubleDataMut(), uniqueVals.data(),
                        uniqueVals.size() * sizeof(double));
        outs[1] = std::move(ID);
    }
}

// ── splitapply ───────────────────────────────────────────────────────
// splitapply(@fn, x [, x2, ...], G) — apply fn to elements of x
// (and x2, ...) grouped by G. Returns a column vector with one entry
// per group (in ascending group-ID order). When fn returns a
// non-scalar the result is concatenated; v1 supports only scalar
// returns from fn.
void splitapply_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("splitapply: requires (@fn, x [, x2, ...], G)",
                     0, 0, "splitapply", "", "m:splitapply:nargin");
    if (!args[0].isFuncHandle())
        throw Error("splitapply: first argument must be a function handle",
                     0, 0, "splitapply", "", "m:splitapply:notHandle");
    const Value &G = args[args.size() - 1];
    const std::size_t nIn = args.size() - 2;   // excluding handle + G
    if (nIn == 0)
        throw Error("splitapply: at least one data array required",
                     0, 0, "splitapply", "", "m:splitapply:noData");
    const std::size_t n = G.numel();
    for (std::size_t k = 0; k < nIn; ++k) {
        if (args[1 + k].numel() != n)
            throw Error("splitapply: data and G must have the same numel",
                         0, 0, "splitapply", "", "m:splitapply:shape");
    }
    auto *mr = ctx.engine->resource();
    // Bucket indices by group ID.
    std::map<int, std::vector<std::size_t>> buckets;
    for (std::size_t i = 0; i < n; ++i) {
        const int gid = (int)G.elemAsDouble(i);
        buckets[gid].push_back(i);
    }
    auto out = Value::matrix(buckets.size(), 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    std::size_t row = 0;
    for (const auto &[gid, idxs] : buckets) {
        (void)gid;
        // Build per-input subset for this group.
        std::vector<Value> callArgs(nIn);
        for (std::size_t k = 0; k < nIn; ++k) {
            const Value &src = args[1 + k];
            auto sub = Value::matrix(idxs.size(), 1, ValueType::DOUBLE, mr);
            double *sd = sub.doubleDataMut();
            for (std::size_t j = 0; j < idxs.size(); ++j)
                sd[j] = src.elemAsDouble(idxs[j]);
            callArgs[k] = std::move(sub);
        }
        Value r = ctx.engine->callFunctionHandle(
            args[0],
            Span<const Value>(callArgs.data(), callArgs.size()),
            ctx.env);
        dst[row++] = r.toScalar();
    }
    outs[0] = std::move(out);
}

// ── groupcounts ──────────────────────────────────────────────────────
// [C, GR, P] = groupcounts(g)
//   C  — column vector of counts, one entry per unique value of g.
//   GR — column of representative values (sorted-unique; NaN trailing
//        if g contains any NaN, matching MATLAB R2025b).
//   P  — column of percentages (100 * count / total).
void groupcounts_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("groupcounts: requires 1 argument",
                     0, 0, "groupcounts", "", "m:groupcounts:nargin");
    auto *mr = ctx.engine->resource();
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(args[0], groups, uniqueVals);
    // Count NaN entries separately.
    std::size_t nan_count = 0;
    for (auto g : groups) if (g == 0) ++nan_count;
    const bool have_nan = nan_count > 0;
    const std::size_t nGroups = uniqueVals.size() + (have_nan ? 1 : 0);
    if (nGroups == 0) {
        outs[0] = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        if (nargout >= 2) outs[1] = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        if (nargout >= 3) outs[2] = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return;
    }
    std::vector<std::size_t> counts(nGroups, 0);
    for (auto g : groups) {
        if (g == 0)        counts[uniqueVals.size()]++;  // trailing NaN bucket
        else               counts[g - 1]++;
    }
    auto out = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < nGroups; ++i)
        dst[i] = double(counts[i]);
    outs[0] = std::move(out);

    if (nargout >= 2) {
        auto GR = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
        double *gd = GR.doubleDataMut();
        for (std::size_t i = 0; i < uniqueVals.size(); ++i)
            gd[i] = uniqueVals[i];
        if (have_nan)
            gd[uniqueVals.size()] = std::numeric_limits<double>::quiet_NaN();
        outs[1] = std::move(GR);
    }
    if (nargout >= 3) {
        const double total = double(groups.size());
        auto P = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
        double *pd = P.doubleDataMut();
        for (std::size_t i = 0; i < nGroups; ++i)
            pd[i] = (total > 0.0) ? 100.0 * double(counts[i]) / total : 0.0;
        outs[2] = std::move(P);
    }
}

// ── groupsummary ────────────────────────────────────────────────────
// Array form:
//   [B, BG, BC] = groupsummary(A, G, method)
//
//   A      column vector or matrix (DOUBLE).
//   G      column vector of grouping values; same length as size(A,1).
//   method scalar string: "sum" | "mean" | "median" | "max" | "min" |
//          "std" | "var" | "numunique" | "nnz" | "mode" | "all" | "any"
//
//   B   nGroups × cols of A
//   BG  column vector of unique group representatives (NaN trailing)
//   BC  column vector of element counts per group
//
// Table form, groupbins, function-handle methods, multi-grouping vars,
// IncludeMissingGroups/IncludeEmptyGroups NV — deferred (table type
// not in numkit; binning + function-handle paths require additional
// engine plumbing).
void groupsummary_reg(Span<const Value> args, size_t nargout,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("groupsummary: requires (A, groupvars, method) "
                    "in this revision (table inputs + groupbins NV "
                    "deferred)",
                    0, 0, "groupsummary", "", "m:groupsummary:nargin");
    if (!args[2].isChar() && !args[2].isString())
        throw Error("groupsummary: method must be a string in this "
                    "revision (function-handle methods deferred)",
                    0, 0, "groupsummary", "", "m:groupsummary:method");
    auto *mr = ctx.engine->resource();
    const Value &A = args[0];
    const Value &G = args[1];
    const std::string method = args[2].toString();

    const std::size_t nRows = A.dims().rows();
    const std::size_t nCols = (A.dims().ndim() >= 2) ? A.dims().cols() : 1;
    if (G.numel() != nRows)
        throw Error("groupsummary: groupvars must have length "
                    "size(A, 1)",
                    0, 0, "groupsummary", "", "m:groupsummary:shape");

    // Group the rows.
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(G, groups, uniqueVals);
    std::size_t nan_count = 0;
    for (auto g : groups) if (g == 0) ++nan_count;
    const bool have_nan = nan_count > 0;
    const std::size_t nGroups = uniqueVals.size() + (have_nan ? 1 : 0);

    // Bucket row indices by group ID (0 = NaN bucket → group index
    // uniqueVals.size()).
    std::vector<std::vector<std::size_t>> buckets(nGroups);
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const std::size_t gi = (groups[i] == 0) ? uniqueVals.size()
                                                : (groups[i] - 1);
        buckets[gi].push_back(i);
    }

    // Allocate B as nGroups × nCols.
    auto B = (nCols == 1) ? Value::matrix(nGroups, 1, ValueType::DOUBLE, mr)
                          : Value::matrix(nGroups, nCols, ValueType::DOUBLE, mr);
    double *bd = B.doubleDataMut();

    // Per-group, per-column reduction.
    auto Aget = [&](std::size_t r, std::size_t c) {
        return A.elemAsDouble(r + c * nRows);
    };

    for (std::size_t c = 0; c < nCols; ++c) {
        for (std::size_t g = 0; g < nGroups; ++g) {
            const auto &rows = buckets[g];
            const std::size_t kn = rows.size();
            double out = 0.0;
            if (method == "sum") {
                for (auto r : rows) out += Aget(r, c);
            } else if (method == "mean") {
                if (kn == 0) out = std::numeric_limits<double>::quiet_NaN();
                else {
                    double s = 0.0;
                    for (auto r : rows) s += Aget(r, c);
                    out = s / double(kn);
                }
            } else if (method == "median") {
                std::vector<double> v; v.reserve(kn);
                for (auto r : rows) v.push_back(Aget(r, c));
                std::sort(v.begin(), v.end());
                out = (kn == 0)            ? std::numeric_limits<double>::quiet_NaN()
                    : (kn % 2 == 1)        ? v[kn / 2]
                                            : 0.5 * (v[kn / 2 - 1] + v[kn / 2]);
            } else if (method == "max") {
                out = -std::numeric_limits<double>::infinity();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (v > out) out = v;
                }
                if (kn == 0) out = std::numeric_limits<double>::quiet_NaN();
            } else if (method == "min") {
                out = std::numeric_limits<double>::infinity();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (v < out) out = v;
                }
                if (kn == 0) out = std::numeric_limits<double>::quiet_NaN();
            } else if (method == "std" || method == "var") {
                if (kn < 2) out = std::numeric_limits<double>::quiet_NaN();
                else {
                    double s = 0.0, ss = 0.0;
                    for (auto r : rows) { double v = Aget(r, c); s += v; ss += v*v; }
                    const double m = s / double(kn);
                    const double v = (ss - double(kn) * m * m) / double(kn - 1);
                    out = (method == "var") ? std::max(0.0, v)
                                             : std::sqrt(std::max(0.0, v));
                }
            } else if (method == "numunique") {
                std::vector<double> v; v.reserve(kn);
                for (auto r : rows) v.push_back(Aget(r, c));
                std::sort(v.begin(), v.end());
                v.erase(std::unique(v.begin(), v.end()), v.end());
                out = double(v.size());
            } else if (method == "nnz") {
                std::size_t k = 0;
                for (auto r : rows) if (Aget(r, c) != 0.0) ++k;
                out = double(k);
            } else if (method == "mode") {
                std::vector<double> v; v.reserve(kn);
                for (auto r : rows) v.push_back(Aget(r, c));
                std::sort(v.begin(), v.end());
                std::size_t best_run = 0, cur_run = 0;
                double best_val = std::numeric_limits<double>::quiet_NaN();
                for (std::size_t i = 0; i < v.size(); ++i) {
                    if (i == 0 || v[i] != v[i - 1]) cur_run = 1;
                    else                            ++cur_run;
                    if (cur_run > best_run) { best_run = cur_run; best_val = v[i]; }
                }
                out = best_val;
            } else if (method == "all") {
                bool ok = true;
                for (auto r : rows) if (Aget(r, c) == 0.0) { ok = false; break; }
                out = ok ? 1.0 : 0.0;
            } else if (method == "any") {
                bool ok = false;
                for (auto r : rows) if (Aget(r, c) != 0.0) { ok = true; break; }
                out = ok ? 1.0 : 0.0;
            } else {
                throw Error("groupsummary: method '" + method
                            + "' not supported in this revision",
                            0, 0, "groupsummary", "",
                            "m:groupsummary:badMethod");
            }
            bd[g + c * nGroups] = out;
        }
    }
    outs[0] = std::move(B);

    if (nargout >= 2) {
        auto BG = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
        double *gd = BG.doubleDataMut();
        for (std::size_t i = 0; i < uniqueVals.size(); ++i) gd[i] = uniqueVals[i];
        if (have_nan)
            gd[uniqueVals.size()] = std::numeric_limits<double>::quiet_NaN();
        outs[1] = std::move(BG);
    }
    if (nargout >= 3) {
        auto BC = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
        double *cd = BC.doubleDataMut();
        for (std::size_t g = 0; g < nGroups; ++g) cd[g] = double(buckets[g].size());
        outs[2] = std::move(BC);
    }
}

// ── grouptransform ──────────────────────────────────────────────────
// Array form:
//   [B, BG] = grouptransform(A, G, method)
//
//   method:  "zscore" | "norm" | "meancenter" | "rescale" |
//            "meanfill" | "linearfill" | function handle.
//
// String methods are applied per group, per column. Function-handle
// methods call back into the engine with the group slice; the
// returned vector must have the same length as the group (we
// concatenate scalar / vector results, matching MATLAB).
//
// NaN values in `G` form a single trailing "NaN group" — matches
// MATLAB's IncludeMissingGroups=true default.
//
// Deferred: table inputs, groupbins, IncludedEdge / ReplaceValues
// NV, multi-grouping-vars (require table type / engine plumbing).
void grouptransform_reg(Span<const Value> args, size_t nargout,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("grouptransform: requires (A, groupvars, method)",
                    0, 0, "grouptransform", "",
                    "m:grouptransform:nargin");
    auto *mr = ctx.engine->resource();
    const Value &A = args[0];
    const Value &G = args[1];
    const Value &methodV = args[2];

    const std::size_t nRows = A.dims().rows();
    const std::size_t nCols = (A.dims().ndim() >= 2) ? A.dims().cols() : 1;
    if (G.numel() != nRows)
        throw Error("grouptransform: groupvars must have length size(A,1)",
                    0, 0, "grouptransform", "",
                    "m:grouptransform:shape");

    // Group the rows. NaN entries → group 0 (sentinel); place them
    // in a trailing bucket.
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(G, groups, uniqueVals);
    std::size_t nan_count = 0;
    for (auto g : groups) if (g == 0) ++nan_count;
    const bool have_nan = nan_count > 0;
    const std::size_t nGroups = uniqueVals.size() + (have_nan ? 1 : 0);
    std::vector<std::vector<std::size_t>> buckets(nGroups);
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const std::size_t gi = (groups[i] == 0) ? uniqueVals.size()
                                                : (groups[i] - 1);
        buckets[gi].push_back(i);
    }

    // Allocate output B same shape as A.
    auto B = (nCols == 1) ? Value::matrix(nRows, 1, ValueType::DOUBLE, mr)
                          : Value::matrix(nRows, nCols, ValueType::DOUBLE, mr);
    double *bd = B.doubleDataMut();
    // Initialise to NaN so any path that doesn't write a row leaves
    // the value as NaN (matches MATLAB for can't-fill cases).
    for (std::size_t i = 0; i < nRows * nCols; ++i)
        bd[i] = std::numeric_limits<double>::quiet_NaN();

    auto Aget = [&](std::size_t r, std::size_t c) {
        return A.elemAsDouble(r + c * nRows);
    };

    const bool is_handle = methodV.isFuncHandle();
    std::string method;
    if (!is_handle) {
        if (!methodV.isChar() && !methodV.isString())
            throw Error("grouptransform: method must be a string or "
                        "function handle",
                        0, 0, "grouptransform", "",
                        "m:grouptransform:method");
        method = methodV.toString();
        if (method != "zscore" && method != "norm" &&
            method != "meancenter" && method != "rescale" &&
            method != "meanfill" && method != "linearfill")
            throw Error("grouptransform: method must be 'zscore', "
                        "'norm', 'meancenter', 'rescale', 'meanfill', "
                        "'linearfill', or a function handle",
                        0, 0, "grouptransform", "",
                        "m:grouptransform:badMethod");
    }

    for (std::size_t c = 0; c < nCols; ++c) {
        for (const auto &rows : buckets) {
            const std::size_t kn = rows.size();
            if (kn == 0) continue;

            if (is_handle) {
                // Build slice vector, callFunctionHandle, splice back.
                auto sub = Value::matrix(kn, 1, ValueType::DOUBLE, mr);
                double *sd = sub.doubleDataMut();
                for (std::size_t j = 0; j < kn; ++j) sd[j] = Aget(rows[j], c);
                std::array<Value, 1> callArgs{sub};
                Value r = ctx.engine->callFunctionHandle(
                    methodV, Span<const Value>(callArgs.data(), 1),
                    ctx.env);
                // r expected length kn OR scalar (broadcast).
                const std::size_t rn = r.numel();
                if (rn == kn) {
                    for (std::size_t j = 0; j < kn; ++j)
                        bd[rows[j] + c * nRows] = r.elemAsDouble(j);
                } else if (rn == 1) {
                    const double v = r.toScalar();
                    for (std::size_t j = 0; j < kn; ++j)
                        bd[rows[j] + c * nRows] = v;
                } else {
                    throw Error("grouptransform: function handle must "
                                "return a scalar or a vector of the "
                                "same length as the group",
                                0, 0, "grouptransform", "",
                                "m:grouptransform:handleSize");
                }
                continue;
            }

            // String methods.
            if (method == "meancenter" || method == "zscore" ||
                method == "norm") {
                double sum = 0.0, sq = 0.0;
                std::size_t k = 0;
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (!std::isnan(v)) { sum += v; sq += v*v; ++k; }
                }
                const double mean = (k > 0) ? sum / double(k) : 0.0;
                if (method == "meancenter") {
                    for (auto r : rows)
                        bd[r + c * nRows] = Aget(r, c) - mean;
                } else if (method == "zscore") {
                    const double var = (k > 1)
                        ? (sq - double(k) * mean * mean) / double(k - 1)
                        : 0.0;
                    const double sd  = std::sqrt(std::max(0.0, var));
                    for (auto r : rows)
                        bd[r + c * nRows] = (sd > 0.0)
                            ? (Aget(r, c) - mean) / sd
                            : 0.0;
                } else {  // norm — 2-norm of group
                    const double nm = std::sqrt(sq);
                    for (auto r : rows)
                        bd[r + c * nRows] = (nm > 0.0)
                            ? Aget(r, c) / nm : 0.0;
                }
            } else if (method == "rescale") {
                double lo = std::numeric_limits<double>::infinity();
                double hi = -std::numeric_limits<double>::infinity();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (std::isnan(v)) continue;
                    if (v < lo) lo = v;
                    if (v > hi) hi = v;
                }
                const double range = hi - lo;
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (std::isnan(v))            bd[r + c * nRows] = v;
                    else if (range == 0.0)         bd[r + c * nRows] = 0.0;
                    else                            bd[r + c * nRows] = (v - lo) / range;
                }
            } else if (method == "meanfill") {
                double sum = 0.0;
                std::size_t k = 0;
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (!std::isnan(v)) { sum += v; ++k; }
                }
                const double mean = (k > 0) ? sum / double(k)
                                            : std::numeric_limits<double>::quiet_NaN();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    bd[r + c * nRows] = std::isnan(v) ? mean : v;
                }
            } else if (method == "linearfill") {
                // Within group, treat group positions as 1..kn,
                // linearly interpolate NaN entries from the flanking
                // non-NaN values (extrapolate from the closest pair
                // for leading/trailing NaNs; if <2 good values, leave
                // NaNs alone — matches MATLAB R2025b).
                std::vector<double> vals(kn);
                for (std::size_t j = 0; j < kn; ++j) vals[j] = Aget(rows[j], c);
                std::vector<std::size_t> good;
                good.reserve(kn);
                for (std::size_t j = 0; j < kn; ++j)
                    if (!std::isnan(vals[j])) good.push_back(j);
                std::vector<double> out = vals;
                if (good.size() >= 2) {
                    for (std::size_t k = 0; k + 1 < good.size(); ++k) {
                        const std::size_t a = good[k];
                        const std::size_t b = good[k + 1];
                        if (b == a + 1) continue;
                        const double slope = (vals[b] - vals[a]) / double(b - a);
                        for (std::size_t j = a + 1; j < b; ++j)
                            out[j] = vals[a] + slope * double(j - a);
                    }
                    if (good.front() > 0) {
                        const std::size_t a = good[0];
                        const std::size_t b = good[1];
                        const double slope = (vals[b] - vals[a]) / double(b - a);
                        for (std::size_t j = 0; j < a; ++j)
                            out[j] = vals[a] - slope * double(a - j);
                    }
                    if (good.back() + 1 < kn) {
                        const std::size_t a = good[good.size() - 2];
                        const std::size_t b = good[good.size() - 1];
                        const double slope = (vals[b] - vals[a]) / double(b - a);
                        for (std::size_t j = b + 1; j < kn; ++j)
                            out[j] = vals[b] + slope * double(j - b);
                    }
                }
                for (std::size_t j = 0; j < kn; ++j)
                    bd[rows[j] + c * nRows] = out[j];
            }
        }
    }
    outs[0] = std::move(B);

    if (nargout >= 2) {
        // BG matches input G's shape (same length, returned as
        // column).
        auto BG = Value::matrix(nRows, 1, ValueType::DOUBLE, mr);
        double *gd = BG.doubleDataMut();
        for (std::size_t i = 0; i < nRows; ++i)
            gd[i] = G.elemAsDouble(i);
        outs[1] = std::move(BG);
    }
}

} // namespace detail
} // namespace numkit::builtin
