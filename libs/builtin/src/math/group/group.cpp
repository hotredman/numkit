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
// unique value list (length = max(out_groups)).
void groupOf(const Value &g, std::vector<std::size_t> &out_groups,
             std::vector<double> &out_unique)
{
    const std::size_t n = g.numel();
    out_groups.assign(n, 0);
    if (n == 0) { out_unique.clear(); return; }

    std::vector<double> vals(n);
    for (std::size_t i = 0; i < n; ++i) vals[i] = g.elemAsDouble(i);
    std::vector<double> sorted = vals;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end(),
                              [](double a, double b) {
                                  return a == b;
                              }), sorted.end());
    out_unique = sorted;
    for (std::size_t i = 0; i < n; ++i) {
        const auto it = std::lower_bound(sorted.begin(), sorted.end(), vals[i]);
        out_groups[i] = (std::size_t)(it - sorted.begin()) + 1;   // 1-based
    }
}

} // namespace

// ── findgroups ───────────────────────────────────────────────────────
// [G, ID] = findgroups(g) — G[i] is the group ID of g[i] (1-based,
// based on sorted-unique order); ID is the column vector of unique
// values, ID[k] = group k's representative.
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
    for (std::size_t i = 0; i < groups.size(); ++i)
        gd[i] = (double)groups[i];
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
// counts = groupcounts(g) — column vector of counts, one entry per
// unique value of g (in sorted order).
void groupcounts_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("groupcounts: requires 1 argument",
                     0, 0, "groupcounts", "", "m:groupcounts:nargin");
    auto *mr = ctx.engine->resource();
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(args[0], groups, uniqueVals);
    if (uniqueVals.empty()) {
        outs[0] = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return;
    }
    std::vector<std::size_t> counts(uniqueVals.size(), 0);
    for (auto g : groups) counts[g - 1]++;
    auto out = Value::matrix(counts.size(), 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < counts.size(); ++i)
        dst[i] = (double)counts[i];
    outs[0] = std::move(out);
}

} // namespace detail
} // namespace numkit::builtin
