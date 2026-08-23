// src/bundle/src/register/builtin/group_reg.cpp
//
// Registration adapters for group operations (findgroups, groupcounts,
// groupsummary, grouptransform, groupfilter, splitapply).

#include <numkit/core/engine.hpp>
#include <numkit/builtin/datafun.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace numkit::builtin::detail {

// ── findgroups ───────────────────────────────────────────────────────
void findgroups_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("findgroups: requires 1 argument",
                     0, 0, "findgroups", "", "numkit:findgroups:nargin");
    FindgroupsResult r = findgroups(args[0], ctx.engine->resource());
    outs[0] = std::move(r.G);
    if (nargout >= 2) outs[1] = std::move(r.ID);
}

// ── splitapply ───────────────────────────────────────────────────────
void splitapply_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("splitapply: requires at least 3 arguments "
                    "(@fn, x, ..., G)",
                    0, 0, "splitapply", "", "numkit:splitapply:nargin");

    const Value &fn = args[0];
    if (!fn.isFuncHandle())
        throw Error("splitapply: first argument must be a function handle",
                    0, 0, "splitapply", "", "numkit:splitapply:notFuncHandle");

    const Value &G = args[args.size() - 1];
    const std::size_t nIn = args.size() - 2;
    const std::size_t n = G.numel();

    for (std::size_t k = 0; k < nIn; ++k) {
        if (args[1 + k].numel() != n)
            throw Error("splitapply: all data arguments must have the "
                        "same number of elements as G",
                        0, 0, "splitapply", "", "numkit:splitapply:shape");
    }

    std::map<int, std::vector<std::size_t>> buckets;
    for (std::size_t i = 0; i < n; ++i) {
        double gd = G.elemAsDouble(i);
        if (std::isnan(gd)) continue;
        int gid = static_cast<int>(gd);
        if (gid <= 0) continue;
        buckets[gid].push_back(i);
    }

    if (buckets.empty()) {
        outs[0] = Value::matrix(0, 1, ValueType::DOUBLE, ctx.engine->resource());
        return;
    }

    std::size_t nGroups = buckets.size();
    auto result = Value::matrix(nGroups, 1, ValueType::DOUBLE, ctx.engine->resource());
    double *rd = result.doubleDataMut();

    std::size_t gIdx = 0;
    for (const auto &kv : buckets) {
        const auto &idxs = kv.second;
        std::vector<Value> callArgs(nIn);
        for (std::size_t k = 0; k < nIn; ++k) {
            auto sub = Value::matrix(idxs.size(), 1, ValueType::DOUBLE, ctx.engine->resource());
            double *sd = sub.doubleDataMut();
            for (std::size_t j = 0; j < idxs.size(); ++j)
                sd[j] = args[1 + k].elemAsDouble(idxs[j]);
            callArgs[k] = std::move(sub);
        }
        Value r = ctx.engine->callFunctionHandle(
            fn, Span<const Value>(callArgs.data(), callArgs.size()), ctx.env);
        rd[gIdx++] = r.toScalar();
    }

    outs[0] = std::move(result);
}

// ── groupcounts ──────────────────────────────────────────────────────
void groupcounts_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("groupcounts: requires 1 argument",
                    0, 0, "groupcounts", "", "numkit:groupcounts:nargin");
    GroupcountsResult r = groupcounts(args[0], ctx.engine->resource());
    outs[0] = std::move(r.C);
    if (nargout >= 2) outs[1] = std::move(r.GR);
    if (nargout >= 3) outs[2] = std::move(r.P);
}

// ── groupsummary ─────────────────────────────────────────────────────
void groupsummary_reg(Span<const Value> args, size_t nargout,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("groupsummary: requires (A, groupvars, method)",
                    0, 0, "groupsummary", "", "numkit:groupsummary:nargin");

    const Value &m = args[2];
    if (!m.isChar() && !m.isString())
        throw Error("groupsummary: method must be a string",
                    0, 0, "groupsummary", "", "numkit:groupsummary:badMethod");
    std::string method = m.toString();

    GroupsummaryResult r = groupsummary(args[0], args[1], method, ctx.engine->resource());
    outs[0] = std::move(r.B);
    if (nargout >= 2) outs[1] = std::move(r.BG);
    if (nargout >= 3) outs[2] = std::move(r.BC);
}

// ── grouptransform ───────────────────────────────────────────────────
void grouptransform_reg(Span<const Value> args, size_t nargout,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("grouptransform: requires (A, groupvars, method)",
                    0, 0, "grouptransform", "",
                    "numkit:grouptransform:nargin");
    auto *mr = ctx.engine->resource();
    const Value &A = args[0];
    const Value &G = args[1];
    const Value &methodV = args[2];

    const std::size_t nRows = A.dims().rows();
    const std::size_t nCols = (A.dims().ndim() >= 2) ? A.dims().cols() : 1;
    if (G.numel() != nRows)
        throw Error("grouptransform: groupvars must have length size(A,1)",
                    0, 0, "grouptransform", "",
                    "numkit:grouptransform:shape");

    const bool is_handle = methodV.isFuncHandle();
    if (!is_handle) {
        if (!methodV.isChar() && !methodV.isString())
            throw Error("grouptransform: method must be a string or function handle",
                        0, 0, "grouptransform", "",
                        "numkit:grouptransform:method");
        std::string method = methodV.toString();
        outs[0] = grouptransform(A, G, method, mr);
    } else {
        // Function handle callback form
        std::vector<std::size_t> groups;
        std::vector<double> uniqueVals;
        // Group rows using findgroups
        FindgroupsResult fg = findgroups(G, mr);
        const std::size_t n = G.numel();
        std::map<int, std::vector<std::size_t>> buckets;
        for (std::size_t i = 0; i < n; ++i) {
            double v = fg.G.elemAsDouble(i);
            int gid = std::isnan(v) ? 0 : static_cast<int>(v);
            buckets[gid].push_back(i);
        }

        auto B = (nCols == 1) ? Value::matrix(nRows, 1, ValueType::DOUBLE, mr)
                              : Value::matrix(nRows, nCols, ValueType::DOUBLE, mr);
        double *bd = B.doubleDataMut();
        for (std::size_t i = 0; i < nRows * nCols; ++i)
            bd[i] = std::numeric_limits<double>::quiet_NaN();

        auto Aget = [&](std::size_t r, std::size_t c) {
            return A.elemAsDouble(r + c * nRows);
        };

        for (std::size_t c = 0; c < nCols; ++c) {
            for (const auto &kv : buckets) {
                const auto &rows = kv.second;
                const std::size_t kn = rows.size();
                if (kn == 0) continue;
                auto sub = Value::matrix(kn, 1, ValueType::DOUBLE, mr);
                double *sd = sub.doubleDataMut();
                for (std::size_t j = 0; j < kn; ++j) sd[j] = Aget(rows[j], c);
                std::array<Value, 1> callArgs{sub};
                Value r = ctx.engine->callFunctionHandle(
                    methodV, Span<const Value>(callArgs.data(), 1), ctx.env);
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
                                "numkit:grouptransform:handleSize");
                }
            }
        }
        outs[0] = std::move(B);
    }

    if (nargout >= 2) {
        auto BG = Value::matrix(nRows, 1, ValueType::DOUBLE, mr);
        double *gd = BG.doubleDataMut();
        for (std::size_t i = 0; i < nRows; ++i)
            gd[i] = G.elemAsDouble(i);
        outs[1] = std::move(BG);
    }
}

// ── groupfilter ────────────────────────────────────────────────────
void groupfilter_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("groupfilter: requires (A, groupvars, @predicate)",
                    0, 0, "groupfilter", "", "numkit:groupfilter:nargin");
    if (!args[2].isFuncHandle())
        throw Error("groupfilter: predicate must be a function handle",
                    0, 0, "groupfilter", "", "numkit:groupfilter:method");
    auto *mr = ctx.engine->resource();
    const Value &A = args[0];
    const Value &G = args[1];
    const Value &pred = args[2];

    const std::size_t nRows = A.dims().rows();
    const std::size_t nCols = (A.dims().ndim() >= 2) ? A.dims().cols() : 1;
    if (G.numel() != nRows)
        throw Error("groupfilter: groupvars must have length size(A,1)",
                    0, 0, "groupfilter", "", "numkit:groupfilter:shape");

    FindgroupsResult fg = findgroups(G, mr);
    const std::size_t n = G.numel();
    std::map<int, std::vector<std::size_t>> buckets;
    for (std::size_t i = 0; i < n; ++i) {
        double v = fg.G.elemAsDouble(i);
        int gid = std::isnan(v) ? 0 : static_cast<int>(v);
        buckets[gid].push_back(i);
    }

    std::vector<std::uint8_t> keep(nRows, 0);
    auto Aget = [&](std::size_t r, std::size_t c) {
        return A.elemAsDouble(r + c * nRows);
    };

    for (const auto &kv : buckets) {
        const auto &rows = kv.second;
        const std::size_t kn = rows.size();
        if (kn == 0) continue;
        auto sub = (nCols == 1)
            ? Value::matrix(kn, 1, ValueType::DOUBLE, mr)
            : Value::matrix(kn, nCols, ValueType::DOUBLE, mr);
        double *sd = sub.doubleDataMut();
        for (std::size_t c = 0; c < nCols; ++c)
            for (std::size_t j = 0; j < kn; ++j)
                sd[j + c * kn] = Aget(rows[j], c);
        std::array<Value, 1> callArgs{sub};
        Value r = ctx.engine->callFunctionHandle(
            pred, Span<const Value>(callArgs.data(), 1), ctx.env);
        const std::size_t rn = r.numel();
        if (rn == kn) {
            for (std::size_t j = 0; j < kn; ++j)
                keep[rows[j]] = (r.elemAsDouble(j) != 0.0) ? 1u : 0u;
        } else {
            bool all_true = true;
            for (std::size_t j = 0; j < rn; ++j)
                if (r.elemAsDouble(j) == 0.0) { all_true = false; break; }
            const std::uint8_t v = all_true ? 1u : 0u;
            for (std::size_t j = 0; j < kn; ++j)
                keep[rows[j]] = v;
        }
    }

    std::vector<std::size_t> kept_idx;
    kept_idx.reserve(nRows);
    for (std::size_t i = 0; i < nRows; ++i)
        if (keep[i]) kept_idx.push_back(i);
    const std::size_t kRows = kept_idx.size();

    auto B = (nCols == 1) ? Value::matrix(kRows, 1, ValueType::DOUBLE, mr)
                          : Value::matrix(kRows, nCols, ValueType::DOUBLE, mr);
    double *bd = B.doubleDataMut();
    for (std::size_t c = 0; c < nCols; ++c)
        for (std::size_t j = 0; j < kRows; ++j)
            bd[j + c * kRows] = Aget(kept_idx[j], c);
    outs[0] = std::move(B);

    if (nargout >= 2) {
        auto BG = Value::matrix(kRows, 1, ValueType::DOUBLE, mr);
        double *gd = BG.doubleDataMut();
        for (std::size_t j = 0; j < kRows; ++j)
            gd[j] = G.elemAsDouble(kept_idx[j]);
        outs[1] = std::move(BG);
    }
}

} // namespace numkit::builtin::detail
