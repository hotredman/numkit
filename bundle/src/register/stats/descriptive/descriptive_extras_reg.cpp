// toolboxes/stats/src/descriptive/descriptive_extras_reg.cpp
//
// CallContext register half of descriptive/descriptive_extras.cpp (Phase 2b
// compute/register split). Engine-coupled glue over the engine-free compute.
#include <numkit/core/engine.hpp>
#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "descriptive/descriptive_extras_detail.hpp"
#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void prepareCurveData_reg(Span<const Value> args, size_t nargout,
                          Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("prepareCurveData: requires (X, Y[, W])",
                    0, 0, "prepareCurveData", "", "numkit:prepCD:nargin");
    auto *mr = ctx.engine->resource();
    Value w_empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &w = (args.size() >= 3) ? args[2] : w_empty;
    auto [xo, yo, wo] = prepareCurveData(args[0], args[1], w, mr);
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(wo);
}

void prepareSurfaceData_reg(Span<const Value> args, size_t nargout,
                            Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("prepareSurfaceData: requires (X, Y, Z)",
                    0, 0, "prepareSurfaceData", "", "numkit:prepSD:nargin");
    auto [xo, yo, zo] = prepareSurfaceData(args[0], args[1], args[2], ctx.engine->resource());
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(zo);
}

void ksdensity_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ksdensity: requires (x[, pts][, N-V pairs])",
                    0, 0, "ksdensity", "", "numkit:ksdensity:nargin");
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
                                0, 0, "ksdensity", "", "numkit:ksdensity:bw");
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
                            0, 0, "ksdensity", "", "numkit:ksdensity:nyi");
        }
        // 'PlotFcn' silently ignored (no-op headless).
        i += 2;
    }
    auto R = ksdensity_full(args[0], pts, bw_user, kernel, function_mode, numpoints, weights, mr);
    outs[0] = std::move(R.f);
    if (nargout > 1) outs[1] = std::move(R.xi);
    if (nargout > 2) outs[2] = std::move(R.bw);
}

void datastats_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("datastats: requires X[, Y]",
                    0, 0, "datastats", "", "numkit:datastats:nargin");
    auto build = [&](const Value &v) {
        auto [num, mx, mn, me, md, rg, sd] =
            datastats(v, ctx.engine->resource());
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
                     0, 0, "bounds", "", "numkit:bounds:nargin");
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
                             0, 0, "bounds", "", "numkit:bounds:badFlag");
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
                                0, 0, "bounds", "", "numkit:bounds:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("bounds: partial vecdim reduction is not yet "
                            "supported (only full-flatten vecdim)",
                            0, 0, "bounds", "", "numkit:bounds:vecdim");
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
        auto [lo, hi] = bounds(flat, 2, mr);
        outs[0] = std::move(lo);
        if (nargout > 1) outs[1] = std::move(hi);
    } else {
        auto [lo, hi] = bounds(args[0], dim, mr);
        outs[0] = std::move(lo);
        if (nargout > 1) outs[1] = std::move(hi);
    }
}

void iqr_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iqr: requires at least 1 argument",
                     0, 0, "iqr", "", "numkit:iqr:nargin");
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
                              0, 0, "iqr", "", "numkit:iqr:badFlag");
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
                                0, 0, "iqr", "", "numkit:iqr:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("iqr: partial vecdim reduction is not yet "
                            "supported (only full-flatten vecdim like [1 2])",
                            0, 0, "iqr", "", "numkit:iqr:vecdim");
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
        outs[0] = iqr(flat, 2, mr);
    } else {
        outs[0] = iqr(args[0], dim, mr);
    }
}

void maxk_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("maxk: requires at least 2 arguments (x, k)",
                     0, 0, "maxk", "", "numkit:maxk:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    int dim = 0;
    // Optional positional dim (numeric scalar that's not a string).
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    // Remaining args may be Name-Value pairs; only 'ComparisonMethod'
    // is documented (real|abs|auto). For real input 'auto' = 'real'; 'abs'
    // ranks by magnitude |x| (returning the original signed values).
    bool byAbs = false;
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("maxk: expected Name-Value pair",
                        0, 0, "maxk", "", "numkit:maxk:nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (name == "comparisonmethod") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (m != "real" && m != "abs" && m != "auto")
                throw Error("maxk: ComparisonMethod must be 'real', 'abs' or 'auto'",
                            0, 0, "maxk", "", "numkit:maxk:cm");
            byAbs = (m == "abs");
        } else {
            throw Error("maxk: unknown Name-Value '" + name + "'",
                        0, 0, "maxk", "", "numkit:maxk:nv");
        }
        i += 2;
    }
    auto *mr = ctx.engine->resource();
    Value idx;
    Value *idxPtr = (nargout >= 2) ? &idx : nullptr;
    outs[0] = topKAlongDim(args[0], dim, k, /*ascending=*/false, "maxk", mr, idxPtr, byAbs);
    if (nargout >= 2) outs[1] = std::move(idx);
}

void mink_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mink: requires at least 2 arguments (x, k)",
                     0, 0, "mink", "", "numkit:mink:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    int dim = 0;
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    bool byAbs = false;
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("mink: expected Name-Value pair",
                        0, 0, "mink", "", "numkit:mink:nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (name == "comparisonmethod") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (m != "real" && m != "abs" && m != "auto")
                throw Error("mink: ComparisonMethod must be 'real', 'abs' or 'auto'",
                            0, 0, "mink", "", "numkit:mink:cm");
            byAbs = (m == "abs");
        } else {
            throw Error("mink: unknown Name-Value '" + name + "'",
                        0, 0, "mink", "", "numkit:mink:nv");
        }
        i += 2;
    }
    auto *mr = ctx.engine->resource();
    Value idx;
    Value *idxPtr = (nargout >= 2) ? &idx : nullptr;
    outs[0] = topKAlongDim(args[0], dim, k, /*ascending=*/true, "mink", mr, idxPtr, byAbs);
    if (nargout >= 2) outs[1] = std::move(idx);
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
                    0, 0, fn, "", std::string("numkit:") + fn + ":badFlag");
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
                        0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
        seen[d] = true;
    }
    bool allCovered = true;
    for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
    if (!allCovered)
        throw Error(std::string(fn) + ": partial vecdim reduction not supported",
                    0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
    flatten = true;
}

Value flattenToRow(const Value &x, std::pmr::memory_resource *mr)
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
                     0, 0, "mape", "", "numkit:mape:nargin");
    int dim = 0; bool flatten = false;
    parseDimOrAll(args[0], args, 2, dim, flatten, "mape");
    auto *mr = ctx.engine->resource();
    if (flatten) {
        outs[0] = mape(flattenToRow(args[0], mr), flattenToRow(args[1], mr), 2, mr);
    } else {
        outs[0] = mape(args[0], args[1], dim, mr);
    }
}

void rmse_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rmse: requires at least 2 arguments (F, A)",
                     0, 0, "rmse", "", "numkit:rmse:nargin");
    int dim = 0; bool flatten = false;
    parseDimOrAll(args[0], args, 2, dim, flatten, "rmse");
    auto *mr = ctx.engine->resource();
    if (flatten) {
        outs[0] = rmse(flattenToRow(args[0], mr), flattenToRow(args[1], mr), 2, mr);
    } else {
        outs[0] = rmse(args[0], args[1], dim, mr);
    }
}

void ecdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ecdf: requires (y[, N-V pairs])",
                     0, 0, "ecdf", "", "numkit:ecdf:nargin");
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
                            0, 0, "ecdf", "", "numkit:ecdf:censoring_nyi");
        }
        else if (key == "iterationlimit" || key == "tolerance"
                 || key == "icmfrequency" || key == "bounds") {
            // Silently accepted (no-op for non-censored ecdf).
        }
    }
    const bool want_bounds = (nargout > 2);
    auto R = ecdf_full(args[0], freq, function_mode, alpha, want_bounds, mr);
    outs[0] = std::move(R.f);
    if (nargout > 1) outs[1] = std::move(R.x);
    if (nargout > 2) outs[2] = std::move(R.flo);
    if (nargout > 3) outs[3] = std::move(R.fup);
}

void ecdfhist_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ecdfhist: requires (f, x [, m])",
                     0, 0, "ecdfhist", "", "numkit:ecdfhist:nargin");
    int m = 10;
    if (args.size() >= 3 && !args[2].isEmpty())
        m = static_cast<int>(args[2].toScalar());
    auto [n, c] = ecdfhist(args[0], args[1], m, ctx.engine->resource());
    outs[0] = std::move(n);
    if (nargout > 1) outs[1] = std::move(c);
}

// ── partialcorr adapter ──────────────────────────────────────────────

void partialcorr_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty())
        throw Error("partialcorr: requires (X), (X, Z), or (X, Y, Z)",
                    0, 0, "partialcorr", "", "numkit:partialcorr:nargin");
    // Positional matrices precede the trailing Name-Value pairs (which are
    // all strings: 'Rows'/'Type' plus their values).
    std::size_t posN = args.size();
    while (posN > 0 && (args[posN - 1].isChar() || args[posN - 1].isString()))
        --posN;
    if (posN < 1 || posN > 3)
        throw Error("partialcorr: requires (X), (X, Z), or (X, Y, Z)",
                    0, 0, "partialcorr", "", "numkit:partialcorr:nargin");

    // Parse the 'Rows' NaN policy from the NV region (args[posN..]):
    //   'all' (default) NaN-poison, 'complete' listwise deletion.
    int rowsMode = 0;  // 0=all, 1=complete, 2=pairwise
    for (std::size_t i = posN; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "rows") {
            std::string v = args[i + 1].toString();
            for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "all") rowsMode = 0;
            else if (v == "complete") rowsMode = 1;
            else if (v == "pairwise") rowsMode = 2;
            else throw Error("partialcorr: Rows must be 'all', 'complete', or "
                             "'pairwise'", 0, 0, "partialcorr", "",
                             "numkit:partialcorr:BadRows");
        }
    }
    if (rowsMode == 2)
        throw Error("partialcorr: 'pairwise' rows option is not yet supported "
                    "(use 'complete')",
                    0, 0, "partialcorr", "", "numkit:partialcorr:Pairwise");

    auto nrows = [](const Value &M) {
        return (M.dims().isVector() || M.isScalar())
                   ? M.numel() : static_cast<std::size_t>(M.dims().rows());
    };
    auto ncols = [](const Value &M) {
        return (M.dims().isVector() || M.isScalar())
                   ? static_cast<std::size_t>(1)
                   : static_cast<std::size_t>(M.dims().cols());
    };

    Value c0, c1, c2;
    const Value *p0 = &args[0], *p1 = (posN >= 2 ? &args[1] : nullptr),
                *p2 = (posN >= 3 ? &args[2] : nullptr);
    if (rowsMode == 1) {
        // Listwise deletion: drop every row with a NaN in ANY of the
        // positional matrices (they all share the same row index).
        const std::size_t n = nrows(args[0]);
        ScratchArena scratch(mr);
        ScratchVec<std::size_t> keep(&scratch);
        const Value *mats[3] = {p0, p1, p2};
        for (std::size_t r = 0; r < n; ++r) {
            bool ok = true;
            for (std::size_t t = 0; t < posN && ok; ++t) {
                const Value &M = *mats[t];
                const std::size_t p = ncols(M);
                for (std::size_t c = 0; c < p && ok; ++c)
                    if (std::isnan(M.elemAsDouble(r + c * n))) ok = false;
            }
            if (ok) keep.push_back(r);
        }
        const std::size_t m = keep.size();
        auto cleanOne = [&](const Value &M) {
            const std::size_t p = ncols(M);
            Value out = Value::matrix(m, p, ValueType::DOUBLE, mr);
            double *o = out.doubleDataMut();
            for (std::size_t c = 0; c < p; ++c)
                for (std::size_t k = 0; k < m; ++k)
                    o[k + c * m] = M.elemAsDouble(keep[k] + c * n);
            return out;
        };
        c0 = cleanOne(args[0]);
        p0 = &c0;
        if (p1) { c1 = cleanOne(args[1]); p1 = &c1; }
        if (p2) { c2 = cleanOne(args[2]); p2 = &c2; }
    }

    if (posN == 1)
        outs[0] = partialcorr_xx(*p0, mr);
    else if (posN == 2)
        outs[0] = partialcorr_xz(*p0, *p1, mr);
    else
        outs[0] = partialcorr_of(*p0, *p1, *p2, mr);
}

// ── corr / detrend adapters ──────────────────────────────────────────

namespace {

enum class CorrType { Pearson, Spearman, Kendall };

// Parse a 'Type' Name-Value option (case-insensitive) from args[start..].
// Other NV names (Rows/Tail/Weights) are skipped. Default Pearson.
CorrType parseCorrType(Span<const Value> args, std::size_t start)
{
    for (std::size_t i = start; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "type") {
            std::string v = args[i + 1].toString();
            for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "pearson")  return CorrType::Pearson;
            if (v == "spearman") return CorrType::Spearman;
            if (v == "kendall")  return CorrType::Kendall;
            throw Error("corr: Type must be 'Pearson', 'Spearman', or 'Kendall'",
                        0, 0, "corr", "", "numkit:corr:BadType");
        }
    }
    return CorrType::Pearson;
}

// Replace each column of X (n×p) with its tied (average) ranks. Pearson of
// the ranks is exactly the Spearman correlation.
Value rankColumns(const Value &X, std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t p = (X.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(X.dims().dim(1)) : 1;
    Value R = Value::matrix(n, p, ValueType::DOUBLE, mr);
    double *rd = R.doubleDataMut();
    ScratchArena scratch(mr);
    for (std::size_t c = 0; c < p; ++c) {
        ScratchVec<std::size_t> idx(n, static_cast<std::size_t>(0), &scratch);
        for (std::size_t i = 0; i < n; ++i) idx[i] = i;
        std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
            return X.elemAsDouble(a + c * n) < X.elemAsDouble(b + c * n);
        });
        std::size_t i = 0;
        while (i < n) {
            std::size_t j = i;
            const double v = X.elemAsDouble(idx[i] + c * n);
            while (j + 1 < n && X.elemAsDouble(idx[j + 1] + c * n) == v) ++j;
            const double avg =
                (static_cast<double>(i + 1) + static_cast<double>(j + 1)) / 2.0;
            for (std::size_t k = i; k <= j; ++k) rd[idx[k] + c * n] = avg;
            i = j + 1;
        }
    }
    return R;
}

// Kendall tau-b between column ci of X and column cj of Y (length n).
double kendallTauB(const Value &X, std::size_t ci,
                   const Value &Y, std::size_t cj, std::size_t n)
{
    long nc = 0, nd = 0, n1 = 0, n2 = 0; // concordant, discordant, ties in x, ties in y
    for (std::size_t i = 0; i < n; ++i) {
        const double ai = X.elemAsDouble(i + ci * n);
        const double bi = Y.elemAsDouble(i + cj * n);
        for (std::size_t j = i + 1; j < n; ++j) {
            const double da = X.elemAsDouble(j + ci * n) - ai;
            const double db = Y.elemAsDouble(j + cj * n) - bi;
            const bool tiea = (da == 0.0);
            const bool tieb = (db == 0.0);
            if (tiea) ++n1;
            if (tieb) ++n2;
            if (!tiea && !tieb) {
                if ((da > 0.0) == (db > 0.0)) ++nc; else ++nd;
            }
        }
    }
    const double n0 = static_cast<double>(n) * (static_cast<double>(n) - 1.0) / 2.0;
    const double denom = std::sqrt((n0 - static_cast<double>(n1)) *
                                   (n0 - static_cast<double>(n2)));
    if (denom <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    return (static_cast<double>(nc) - static_cast<double>(nd)) / denom;
}

// p×q matrix of Kendall tau-b for every column pair of X (n×p), Y (n×q).
Value kendallMatrix(const Value &X, const Value &Y, std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t p = (X.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(X.dims().dim(1)) : 1;
    if (static_cast<std::size_t>(Y.dims().dim(0)) != n)
        throw Error("corr: X and Y must have the same number of rows",
                    0, 0, "corr", "", "numkit:corr:rows");
    const std::size_t q = (Y.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(Y.dims().dim(1)) : 1;
    Value out = Value::matrix(p, q, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t j = 0; j < q; ++j)
        for (std::size_t i = 0; i < p; ++i)
            od[i + j * p] = kendallTauB(X, i, Y, j, n);
    return out;
}

// ── corr 'Rows' NaN policy ────────────────────────────────────────────
enum class CorrRows { All, Complete, Pairwise };

// Parse a 'Rows' Name-Value option (case-insensitive): 'all' (default,
// NaN-poison), 'complete' (listwise deletion), 'pairwise'.
CorrRows parseCorrRows(Span<const Value> args, std::size_t start)
{
    for (std::size_t i = start; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "rows") {
            std::string v = args[i + 1].toString();
            for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "all")      return CorrRows::All;
            if (v == "complete") return CorrRows::Complete;
            if (v == "pairwise") return CorrRows::Pairwise;
            throw Error("corr: Rows must be 'all', 'complete', or 'pairwise'",
                        0, 0, "corr", "", "numkit:corr:BadRows");
        }
    }
    return CorrRows::All;
}

std::size_t corrRows(const Value &X) { return static_cast<std::size_t>(X.dims().dim(0)); }
std::size_t corrCols(const Value &X)
{
    return (X.dims().ndim() >= 2) ? static_cast<std::size_t>(X.dims().dim(1)) : 1;
}

// Listwise deletion: keep only the rows that contain no NaN across every
// column of X (and Y, when two matrices share a row index). The same kept
// rows are applied to both so the columns stay aligned.
void dropNaNRows(const Value &X, const Value *Y, std::pmr::memory_resource *mr,
                 Value &Xo, Value *Yo)
{
    const std::size_t n = corrRows(X);
    const std::size_t pX = corrCols(X);
    const std::size_t pY = Y ? corrCols(*Y) : 0;
    ScratchArena scratch(mr);
    ScratchVec<std::size_t> keep(&scratch);
    for (std::size_t r = 0; r < n; ++r) {
        bool ok = true;
        for (std::size_t c = 0; c < pX && ok; ++c)
            if (std::isnan(X.elemAsDouble(r + c * n))) ok = false;
        if (ok && Y)
            for (std::size_t c = 0; c < pY && ok; ++c)
                if (std::isnan(Y->elemAsDouble(r + c * n))) ok = false;
        if (ok) keep.push_back(r);
    }
    const std::size_t m = keep.size();
    Xo = Value::matrix(m, pX, ValueType::DOUBLE, mr);
    double *xo = Xo.doubleDataMut();
    for (std::size_t c = 0; c < pX; ++c)
        for (std::size_t k = 0; k < m; ++k)
            xo[k + c * m] = X.elemAsDouble(keep[k] + c * n);
    if (Y && Yo) {
        *Yo = Value::matrix(m, pY, ValueType::DOUBLE, mr);
        double *yo = Yo->doubleDataMut();
        for (std::size_t c = 0; c < pY; ++c)
            for (std::size_t k = 0; k < m; ++k)
                yo[k + c * m] = Y->elemAsDouble(keep[k] + c * n);
    }
}

// Pairwise Pearson correlation: each entry (i,j) uses the rows where both
// column i of X and column j of Y are non-NaN, with the means taken over
// exactly those rows.
Value corrPairwisePearson(const Value &X, const Value &Y, std::pmr::memory_resource *mr)
{
    const std::size_t n = corrRows(X);
    if (corrRows(Y) != n)
        throw Error("corr: X and Y must have the same number of rows",
                    0, 0, "corr", "", "numkit:corr:rows");
    const std::size_t p = corrCols(X), q = corrCols(Y);
    Value out = Value::matrix(p, q, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < q; ++j) {
            double si = 0.0, sj = 0.0;
            std::size_t m = 0;
            for (std::size_t r = 0; r < n; ++r) {
                const double a = X.elemAsDouble(r + i * n);
                const double b = Y.elemAsDouble(r + j * n);
                if (!std::isnan(a) && !std::isnan(b)) { si += a; sj += b; ++m; }
            }
            double rij;
            if (m < 2) {
                rij = std::numeric_limits<double>::quiet_NaN();
            } else {
                const double mi = si / static_cast<double>(m);
                const double mj = sj / static_cast<double>(m);
                double sxy = 0.0, sxx = 0.0, syy = 0.0;
                for (std::size_t r = 0; r < n; ++r) {
                    const double a = X.elemAsDouble(r + i * n);
                    const double b = Y.elemAsDouble(r + j * n);
                    if (std::isnan(a) || std::isnan(b)) continue;
                    const double da = a - mi, db = b - mj;
                    sxy += da * db; sxx += da * da; syy += db * db;
                }
                const double den = std::sqrt(sxx * syy);
                rij = (den > 0.0) ? sxy / den
                                  : std::numeric_limits<double>::quiet_NaN();
            }
            od[i + j * p] = rij;
        }
    return out;
}

Value corrDispatch(bool twoArg, const Value &X, const Value &Y,
                   CorrType ct, std::pmr::memory_resource *mr)
{
    if (twoArg) {
        if (ct == CorrType::Pearson)  return corr_xy(X, Y, mr);
        if (ct == CorrType::Spearman) return corr_xy(rankColumns(X, mr),
                                                     rankColumns(Y, mr), mr);
        return kendallMatrix(X, Y, mr);
    }
    if (ct == CorrType::Pearson)  return corr_xx(X, mr);
    if (ct == CorrType::Spearman) return corr_xx(rankColumns(X, mr), mr);
    return kendallMatrix(X, X, mr);
}

// ── corr p-values (the 2nd output of [r, p] = corr(...)) ─────────────
// Each correlation r_ij is tested against H0: no association. The p-value
// depends only on (r_ij, n, type): Pearson via the t-distribution; Kendall via
// the EXACT permutation (Mahonian inversions) distribution; Spearman via the
// EXACT permutation distribution (small n) with a t-approximation fallback for
// large n. No-ties is assumed for the exact Kendall/Spearman forms.

// Pearson: t = r·√((n-2)/(1-r²)), p = 2·tcdf(-|t|, n-2).
inline double corrPearsonP(double r, std::size_t n, std::pmr::memory_resource *mr)
{
    if (n < 3 || std::isnan(r)) return std::numeric_limits<double>::quiet_NaN();
    if (std::fabs(r) >= 1.0) return 0.0;
    const double df = static_cast<double>(n) - 2.0;
    const double t = r * std::sqrt(df / (1.0 - r * r));
    const double cdf = tcdf(Value::scalar(-std::fabs(t), mr), df, mr).toScalar();
    return std::min(1.0, 2.0 * cdf);
}

// Exact two-sided Kendall p from the normalized inversions distribution
// (probabilities, so no factorial overflow). D = #discordant pairs. The DP
// ping-pongs two arena buffers sized to the max support (Dmax+1) — no
// per-iteration allocation.
inline double corrKendallExactP(double tau, std::size_t n, std::pmr::memory_resource *mr)
{
    if (n < 2 || std::isnan(tau)) return std::numeric_limits<double>::quiet_NaN();
    const std::size_t Dmax = n * (n - 1) / 2;
    long Dobs = std::lround(static_cast<double>(Dmax) * (1.0 - tau) / 2.0);
    if (Dobs < 0) Dobs = 0;
    if (Dobs > static_cast<long>(Dmax)) Dobs = static_cast<long>(Dmax);
    ScratchArena sc(mr);
    ScratchVec<double> prob(Dmax + 1, 0.0, &sc);      // P(#inversions == d)
    ScratchVec<double> next(Dmax + 1, 0.0, &sc);
    prob[0] = 1.0;
    std::size_t curLen = 1;                            // active prefix length
    for (std::size_t k = 1; k < n; ++k) {
        const std::size_t newLen = curLen + k;
        std::fill(next.begin(), next.begin() + newLen, 0.0);
        const double w = 1.0 / static_cast<double>(k + 1);
        for (std::size_t d = 0; d < curLen; ++d) {
            const double pv = prob[d] * w;
            for (std::size_t s = 0; s <= k; ++s) next[d + s] += pv;
        }
        prob.swap(next);                              // O(1), same arena
        curLen = newLen;
    }
    double lower = 0.0, upper = 0.0;
    for (long d = 0; d <= Dobs; ++d) lower += prob[static_cast<std::size_t>(d)];
    for (long d = Dobs; d < static_cast<long>(curLen); ++d)
        upper += prob[static_cast<std::size_t>(d)];
    return std::min(1.0, 2.0 * std::min(lower, upper));
}

// Exact two-sided Spearman p by enumerating permutations (small n).
// D = Σ(rankX_i - rankY_i)²; under H0 it is the displacement of a random perm.
inline double corrSpearmanExactP(double rho, std::size_t n, std::pmr::memory_resource *mr)
{
    const double scale = static_cast<double>(n) * (static_cast<double>(n) * n - 1.0) / 6.0;
    long Dobs = std::lround((1.0 - rho) * scale);
    if (Dobs < 0) Dobs = 0;
    ScratchArena sc(mr);
    ScratchVec<int> perm(n, &sc);
    for (std::size_t i = 0; i < n; ++i) perm[i] = static_cast<int>(i);
    std::size_t total = 0, leCount = 0, geCount = 0;
    do {
        long D = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const long d = perm[i] - static_cast<long>(i);
            D += d * d;
        }
        if (D <= Dobs) ++leCount;
        if (D >= Dobs) ++geCount;
        ++total;
    } while (std::next_permutation(perm.begin(), perm.end()));
    const double lower = static_cast<double>(leCount) / static_cast<double>(total);
    const double upper = static_cast<double>(geCount) / static_cast<double>(total);
    return std::min(1.0, 2.0 * std::min(lower, upper));
}

// Spearman p: exact enumeration for small n, t-approximation otherwise (NOTE:
// MATLAB uses the AS 89 approximation for large n — that tail is approximate).
inline double corrSpearmanP(double rho, std::size_t n, std::pmr::memory_resource *mr)
{
    if (n < 2 || std::isnan(rho)) return std::numeric_limits<double>::quiet_NaN();
    if (n <= 10) return corrSpearmanExactP(rho, n, mr);
    if (std::fabs(rho) >= 1.0) return 0.0;
    const double df = static_cast<double>(n) - 2.0;
    const double t = rho * std::sqrt(df / (1.0 - rho * rho));
    const double cdf = tcdf(Value::scalar(-std::fabs(t), mr), df, mr).toScalar();
    return std::min(1.0, 2.0 * cdf);
}

// Build the p-value matrix (same shape as r). isAuto → corr(X) form, so the
// diagonal (variable with itself) is forced to 1, matching MATLAB.
Value corrPValueMatrix(const Value &r, std::size_t n, CorrType ct, bool isAuto,
                       std::pmr::memory_resource *mr)
{
    const std::size_t p = static_cast<std::size_t>(r.dims().dim(0));
    const std::size_t q = static_cast<std::size_t>(r.dims().dim(1));
    Value P = Value::matrix(p, q, ValueType::DOUBLE, mr);
    double *pd = P.doubleDataMut();
    const double *rd = r.doubleData();
    // Kendall exact uses n only; compute the threshold for the large-n fallback.
    const bool kendallExact = (ct == CorrType::Kendall && n <= 100);
    for (std::size_t j = 0; j < q; ++j)
        for (std::size_t i = 0; i < p; ++i) {
            const double rij = rd[i + j * p];
            if (isAuto && i == j) { pd[i + j * p] = 1.0; continue; }
            double pv;
            if (ct == CorrType::Pearson)
                pv = corrPearsonP(rij, n, mr);
            else if (ct == CorrType::Spearman)
                pv = corrSpearmanP(rij, n, mr);
            else if (kendallExact)
                pv = corrKendallExactP(rij, n, mr);
            else {  // large-n Kendall: tie-free normal approximation
                if (std::isnan(rij)) pv = std::numeric_limits<double>::quiet_NaN();
                else {
                    const double z = 3.0 * rij * std::sqrt(static_cast<double>(n) * (n - 1))
                                   / std::sqrt(2.0 * (2.0 * n + 5.0));
                    // 2·Φ(-|z|) = erfc(|z|/√2)
                    pv = std::min(1.0, std::erfc(std::fabs(z) / std::sqrt(2.0)));
                }
            }
            pd[i + j * p] = pv;
        }
    return P;
}

} // namespace

void corr_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("corr: requires at least 1 argument",
                    0, 0, "corr", "", "numkit:corr:nargin");
    auto *mr = ctx.engine->resource();
    // Distinguish corr(X[, NV…]) from corr(X, Y[, NV…]). Y is the 2nd
    // positional non-string argument.
    const bool twoArg =
        (args.size() >= 2 && !args[1].isChar() && !args[1].isString());
    const std::size_t nvStart = twoArg ? 2 : 1;
    const CorrType ct = parseCorrType(args, nvStart);
    const CorrRows rows = parseCorrRows(args, nvStart);
    const bool isAuto = !twoArg;   // corr(X) auto-correlation → diagonal p = 1

    Value r;
    std::size_t n;
    if (rows == CorrRows::Pairwise) {
        // Pairwise deletion is currently supported for Pearson only.
        if (ct != CorrType::Pearson)
            throw Error("corr: 'pairwise' rows option is supported only for "
                        "the 'Pearson' type",
                        0, 0, "corr", "", "numkit:corr:PairwiseType");
        r = corrPairwisePearson(args[0], twoArg ? args[1] : args[0], mr);
        n = corrRows(args[0]);
    } else if (rows == CorrRows::Complete) {
        // Listwise deletion: drop every row containing a NaN, then compute.
        Value Xc, Yc;
        if (twoArg) {
            dropNaNRows(args[0], &args[1], mr, Xc, &Yc);
            r = corrDispatch(true, Xc, Yc, ct, mr);
        } else {
            dropNaNRows(args[0], nullptr, mr, Xc, nullptr);
            r = corrDispatch(false, Xc, Xc, ct, mr);
        }
        n = corrRows(Xc);
    } else {
        // 'all' (default): NaN-poisoning behaviour, unchanged.
        r = twoArg ? corrDispatch(true, args[0], args[1], ct, mr)
                   : corrDispatch(false, args[0], args[0], ct, mr);
        n = corrRows(args[0]);
    }

    Value pmat;
    if (nargout >= 2) pmat = corrPValueMatrix(r, n, ct, isAuto, mr);
    outs[0] = std::move(r);
    if (nargout >= 2) outs[1] = std::move(pmat);
}

namespace {
// Real / imaginary parts of a COMPLEX value as same-shape DOUBLE arrays + the
// inverse combine. detrend (least-squares trend removal) is linear, so it
// commutes with this split.
Value cplxRealPartDt(const Value &f, std::pmr::memory_resource *mr)
{
    auto r = createLike(f, ValueType::DOUBLE, mr);
    double *d = r.doubleDataMut();
    const Complex *c = f.complexData();
    const size_t n = f.numel();
    for (size_t i = 0; i < n; ++i) d[i] = c[i].real();
    return r;
}
Value cplxImagPartDt(const Value &f, std::pmr::memory_resource *mr)
{
    auto r = createLike(f, ValueType::DOUBLE, mr);
    double *d = r.doubleDataMut();
    const Complex *c = f.complexData();
    const size_t n = f.numel();
    for (size_t i = 0; i < n; ++i) d[i] = c[i].imag();
    return r;
}
Value cplxCombineDt(const Value &re, const Value &im, std::pmr::memory_resource *mr)
{
    auto out = createLike(re, ValueType::COMPLEX, mr);
    Complex *o = out.complexDataMut();
    const double *r = re.doubleData(), *m = im.doubleData();
    const size_t n = out.numel();
    for (size_t i = 0; i < n; ++i) o[i] = Complex(r[i], m[i]);
    return out;
}
} // namespace

void detrend_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("detrend: requires at least 1 argument",
                    0, 0, "detrend", "", "numkit:detrend:nargin");
    int order = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            const auto s = args[1].toString();
            if (s == "constant") order = 0;
            else if (s == "linear") order = 1;
            else throw Error("detrend: string mode must be 'constant' or 'linear'",
                             0, 0, "detrend", "", "numkit:detrend:mode");
        } else {
            order = static_cast<int>(args[1].toScalar());
        }
    }
    // detrend(x, 1, bp): continuous piecewise-linear detrend with
    // breakpoints. Supported for linear (order 1) only — order-0 +
    // breakpoints is a rare, ill-defined MATLAB edge and is deferred
    // (the bp argument is then ignored, matching the prior behaviour).
    const bool hasBP = (args.size() >= 3 && order == 1 && !args[2].isEmpty()
                        && !args[2].isChar() && !args[2].isString());
    std::vector<double> bp;
    if (hasBP) {
        const Value &bpv = args[2];
        bp.reserve(bpv.numel());
        for (std::size_t i = 0; i < bpv.numel(); ++i)
            bp.push_back(bpv.elemAsDouble(i));
    }
    auto *mr = ctx.engine->resource();
    auto runReal = [&](const Value &x) -> Value {
        return hasBP ? detrendBP_of(x, bp, mr) : detrend_of(x, order, mr);
    };
    // Complex: detrend the real + imaginary parts separately, recombine.
    if (args[0].type() == ValueType::COMPLEX) {
        outs[0] = cplxCombineDt(runReal(cplxRealPartDt(args[0], mr)),
                                runReal(cplxImagPartDt(args[0], mr)), mr);
        return;
    }
    outs[0] = runReal(args[0]);
}

// ── missing-data adapters ────────────────────────────────────────────

// Method-aware isoutlier: per-column detection (MATLAB operates per column
// for matrices) via detect_one_column. detectTf is the value detect_one_column
// expects (median/mean: 3 == MATLAB ThresholdFactor; quartiles: 2*MATLAB-tf
// because detect_one_column scales by tf*0.5).
// Iterative Grubbs's-test outlier mask for one column. `alpha` is the
// significance level (MATLAB isoutlier 'grubbs' ThresholdFactor, default
// 0.05). At each step the point with the largest studentized deviation
// G = max|x-mean|/std (std with N-1) is compared to the Grubbs critical
// value G_crit = ((N-1)/sqrt(N))·sqrt(t²/(N-2+t²)) with t = tinv(alpha/(2N),
// N-2); if G > G_crit that point is flagged and removed, then repeat (down
// to N>=3). NaNs are ignored. Matches MATLAB R2025b.
static void grubbsColumnMask(const double *x, std::size_t n, double alpha,
                             uint8_t *maskOut, std::pmr::memory_resource *mr)
{
    for (std::size_t i = 0; i < n; ++i) maskOut[i] = 0;
    std::vector<double> v;
    std::vector<std::size_t> pos;
    v.reserve(n); pos.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isnan(x[i])) { v.push_back(x[i]); pos.push_back(i); }
    while (v.size() >= 3) {
        const std::size_t m = v.size();
        double s = 0.0;
        for (double e : v) s += e;
        const double mean = s / double(m);
        double ss = 0.0;
        for (double e : v) ss += (e - mean) * (e - mean);
        const double sd = std::sqrt(ss / double(m - 1));
        if (!(sd > 0.0)) break;
        std::size_t jmax = 0;
        double gmax = -1.0;
        for (std::size_t j = 0; j < m; ++j) {
            const double g = std::fabs(v[j] - mean) / sd;
            if (g > gmax) { gmax = g; jmax = j; }
        }
        const double dof   = double(m) - 2.0;
        const double pp    = alpha / (2.0 * double(m));
        const double tcrit = tinv(Value::scalar(pp, mr), dof, mr).toScalar();
        const double Gcrit = ((double(m) - 1.0) / std::sqrt(double(m)))
                           * std::sqrt(tcrit * tcrit / (dof + tcrit * tcrit));
        if (gmax > Gcrit) {
            maskOut[pos[jmax]] = 1;
            v.erase(v.begin() + static_cast<std::ptrdiff_t>(jmax));
            pos.erase(pos.begin() + static_cast<std::ptrdiff_t>(jmax));
        } else {
            break;
        }
    }
}

// Generalized ESD (Rosner 1983) outlier test for one column. Like grubbs but
// it does NOT stop at the first non-exceedance: it peels the most-extreme point
// for up to `maxOut` iterations, then flags every point removed up to the
// LARGEST iteration whose studentized deviate R_i exceeds the critical value
// λ_i — which equals the Grubbs critical value at the current sample size m
// (= N-i+1). This handles masking/swamping (multiple outliers inflating s).
// `alpha` is the significance level (ThresholdFactor, default 0.05).
// `maxOutArg < 0` selects the MATLAB default max(1, round(N/10)).
static void gesdColumnMask(const double *x, std::size_t n, double alpha,
                           long maxOutArg, uint8_t *maskOut,
                           std::pmr::memory_resource *mr)
{
    for (std::size_t i = 0; i < n; ++i) maskOut[i] = 0;
    std::vector<double> v;
    std::vector<std::size_t> pos;
    v.reserve(n); pos.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isnan(x[i])) { v.push_back(x[i]); pos.push_back(i); }
    const std::size_t N = v.size();
    if (N < 3) return;
    std::size_t r = (maxOutArg >= 0)
        ? static_cast<std::size_t>(maxOutArg)
        : static_cast<std::size_t>(std::max(1.0, std::round(double(N) / 10.0)));
    if (r > N - 2) r = N - 2;   // need ≥2 points left to form a std

    std::vector<std::size_t> removed;   // position removed at each iteration
    std::vector<char>        exceed;    // R_i > λ_i ?
    removed.reserve(r); exceed.reserve(r);
    for (std::size_t it = 0; it < r; ++it) {
        const std::size_t m = v.size();
        if (m < 3) break;
        double s = 0.0;
        for (double e : v) s += e;
        const double mean = s / double(m);
        double ss = 0.0;
        for (double e : v) ss += (e - mean) * (e - mean);
        const double sd = std::sqrt(ss / double(m - 1));
        if (!(sd > 0.0)) break;
        std::size_t jmax = 0;
        double gmax = -1.0;
        for (std::size_t j = 0; j < m; ++j) {
            const double g = std::fabs(v[j] - mean) / sd;
            if (g > gmax) { gmax = g; jmax = j; }
        }
        const double dof   = double(m) - 2.0;
        const double pp    = alpha / (2.0 * double(m));
        const double tcrit = tinv(Value::scalar(pp, mr), dof, mr).toScalar();
        const double lam   = ((double(m) - 1.0) / std::sqrt(double(m)))
                           * std::sqrt(tcrit * tcrit / (dof + tcrit * tcrit));
        removed.push_back(pos[jmax]);
        exceed.push_back(gmax > lam ? 1 : 0);
        v.erase(v.begin() + static_cast<std::ptrdiff_t>(jmax));
        pos.erase(pos.begin() + static_cast<std::ptrdiff_t>(jmax));
    }
    long last = -1;
    for (long it = 0; it < static_cast<long>(exceed.size()); ++it)
        if (exceed[it]) last = it;
    for (long it = 0; it <= last; ++it) maskOut[removed[it]] = 1;
}

static Value isoutlierMethod(const Value &x, const std::string &method,
                             double detectTf, std::pmr::memory_resource *mr,
                             long hb = 0, long hf = 0, long maxOut = -1)
{
    if (x.numel() == 0) return Value::matrix(0, 0, ValueType::LOGICAL, mr);
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::LOGICAL, mr);
    uint8_t *od = out.logicalDataMut();
    const double *xd = x.doubleData();
    if (method == "movmedian" || method == "movmean") {
        const bool isMed = (method == "movmedian");
        if (r == 1 || c == 1) {
            auto m = detect_moving_column(xd, x.numel(), isMed, hb, hf, detectTf);
            for (std::size_t i = 0; i < x.numel(); ++i) od[i] = m[i];
        } else {
            for (std::size_t col = 0; col < c; ++col) {
                auto m = detect_moving_column(xd + col * r, r, isMed, hb, hf, detectTf);
                for (std::size_t i = 0; i < r; ++i) od[col * r + i] = m[i];
            }
        }
        return out;
    }
    if (method == "grubbs") {
        // Iterative test (not a static lo/hi rule); per-column. detectTf is
        // the significance level alpha.
        if (r == 1 || c == 1) {
            grubbsColumnMask(xd, x.numel(), detectTf, od, mr);
        } else {
            for (std::size_t col = 0; col < c; ++col)
                grubbsColumnMask(xd + col * r, r, detectTf, od + col * r, mr);
        }
        return out;
    }
    if (method == "gesd") {
        // Generalized ESD; per-column. detectTf is the significance level
        // alpha; maxOut is MaxNumOutliers (<0 → MATLAB default).
        if (r == 1 || c == 1) {
            gesdColumnMask(xd, x.numel(), detectTf, maxOut, od, mr);
        } else {
            for (std::size_t col = 0; col < c; ++col)
                gesdColumnMask(xd + col * r, r, detectTf, maxOut, od + col * r, mr);
        }
        return out;
    }
    if (r == 1 || c == 1) {
        // Vector: the whole run is one column.
        FoDetect d = detect_one_column(xd, x.numel(), method, detectTf);
        for (std::size_t i = 0; i < x.numel(); ++i) od[i] = d.mask[i];
    } else {
        for (std::size_t col = 0; col < c; ++col) {
            FoDetect d = detect_one_column(xd + col * r, r, method, detectTf);
            for (std::size_t i = 0; i < r; ++i) od[col * r + i] = d.mask[i];
        }
    }
    return out;
}

void isoutlier_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isoutlier: requires at least 1 argument",
                    0, 0, "isoutlier", "", "numkit:isoutlier:nargin");
    auto *mr = ctx.engine->resource();

    // isoutlier(A[, method][, 'ThresholdFactor', tf]). The method arg was
    // parsed-and-ignored (always median/MAD); now honoured.
    std::string method = "median";
    std::size_t ai = 1;
    long hb = 0, hf = 0;   // moving-window half-spans (movmedian / movmean)
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
        std::string m = args[1].toString();
        for (char &ch : m) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (m == "median" || m == "mean" || m == "quartiles" || m == "grubbs") {
            method = m;
            ai = 2;
        } else if (m == "movmedian" || m == "movmean") {
            method = m;
            // Moving methods require a window as the next positional arg.
            if (args.size() < 3 || args[2].isChar() || args[2].isString())
                throw Error("isoutlier: the '" + m + "' method requires a "
                            "window length (scalar k or [back forward])",
                            0, 0, "isoutlier", "", "numkit:isoutlier:window");
            const Value &win = args[2];
            if (win.numel() == 1) {
                const long k = static_cast<long>(win.toScalar());
                if (k < 1)
                    throw Error("isoutlier: window length must be a positive integer",
                                0, 0, "isoutlier", "", "numkit:isoutlier:window");
                if (k % 2 == 1) { hb = hf = (k - 1) / 2; }
                else { hb = k / 2; hf = k / 2 - 1; }
            } else if (win.numel() == 2) {
                hb = static_cast<long>(win.elemAsDouble(0));
                hf = static_cast<long>(win.elemAsDouble(1));
                if (hb < 0 || hf < 0)
                    throw Error("isoutlier: window [back forward] must be nonnegative",
                                0, 0, "isoutlier", "", "numkit:isoutlier:window");
            } else {
                throw Error("isoutlier: window must be a scalar or a 2-element "
                            "[back forward] vector",
                            0, 0, "isoutlier", "", "numkit:isoutlier:window");
            }
            ai = 3;
        } else if (m == "gesd") {
            method = m;
            ai = 2;
        }
        // else: not a method token — leave as a Name-Value name parsed below.
    }

    // Default ThresholdFactor per method: quartiles 1.5, grubbs/gesd 0.05
    // (significance level), median/mean/movmedian/movmean 3.
    double userTf = (method == "quartiles")          ? 1.5
                  : (method == "grubbs" ||
                     method == "gesd")               ? 0.05
                                                     : 3.0;
    long maxOut = -1;   // gesd MaxNumOutliers (<0 → MATLAB default)
    for (std::size_t i = ai; i + 1 < args.size(); i += 2) {
        if (args[i].isChar() || args[i].isString()) {
            std::string nm = args[i].toString();
            for (char &ch : nm) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (nm == "thresholdfactor")
                userTf = args[i + 1].toScalar();
            else if (nm == "maxnumoutliers") {
                if (method != "gesd")
                    throw Error("isoutlier: 'MaxNumOutliers' applies only to the "
                                "'gesd' method",
                                0, 0, "isoutlier", "", "numkit:isoutlier:option");
                const double mo = args[i + 1].toScalar();
                if (!(mo >= 1.0) || mo != std::floor(mo))
                    throw Error("isoutlier: MaxNumOutliers must be a positive integer",
                                0, 0, "isoutlier", "", "numkit:isoutlier:maxout");
                maxOut = static_cast<long>(mo);
            } else
                throw Error("isoutlier: unknown option '" + args[i].toString() + "'",
                             0, 0, "isoutlier", "", "numkit:isoutlier:option");
        }
    }
    if (userTf < 0.0)
        throw Error("isoutlier: ThresholdFactor must be nonnegative",
                     0, 0, "isoutlier", "", "numkit:isoutlier:tf");

    const double detectTf = (method == "quartiles") ? 2.0 * userTf : userTf;
    outs[0] = isoutlierMethod(args[0], method, detectTf, mr, hb, hf, maxOut);
}

// Per-column outlier mask for rmoutliers. Adds the rmoutliers-specific
// 'percentiles' method (NOT in detect_one_column): elements below the
// loP-th percentile or above the hiP-th percentile are outliers, using
// MATLAB's prctile convention (sorted positions at 100*(k-0.5)/n, clamped
// at the ends). median/mean/quartiles delegate to detect_one_column.
// Returns a column-major mask of x.numel() bytes (1 == outlier).
static std::vector<uint8_t> rmoutlierMask(const Value &x, const std::string &method,
                                          double loP, double hiP, double detectTf)
{
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    const std::size_t n = x.numel();
    std::vector<uint8_t> mask(n, 0);
    if (n == 0) return mask;
    const double *xd = x.doubleData();

    auto fill_col = [&](const double *col, std::size_t len, uint8_t *m) {
        if (method == "percentiles") {
            std::vector<double> buf;
            buf.reserve(len);
            for (std::size_t i = 0; i < len; ++i)
                if (!std::isnan(col[i])) buf.push_back(col[i]);
            if (buf.empty()) return;
            std::sort(buf.begin(), buf.end());
            auto prc = [&](double p) {
                const double q = p / 100.0 * double(buf.size()) - 0.5;
                if (q <= 0.0) return buf.front();
                if (q >= double(buf.size() - 1)) return buf.back();
                const std::size_t f = static_cast<std::size_t>(std::floor(q));
                const double fr = q - double(f);
                return buf[f] + fr * (buf[f + 1] - buf[f]);
            };
            const double lo = prc(loP);
            const double hi = prc(hiP);
            for (std::size_t i = 0; i < len; ++i)
                m[i] = (std::isnan(col[i]) ? 0
                      : ((col[i] < lo || col[i] > hi) ? 1 : 0));
        } else {
            FoDetect d = detect_one_column(col, len, method, detectTf);
            for (std::size_t i = 0; i < len; ++i) m[i] = d.mask[i];
        }
    };

    if (r == 1 || c == 1) {
        fill_col(xd, n, mask.data());
    } else {
        for (std::size_t col = 0; col < c; ++col)
            fill_col(xd + col * r, r, mask.data() + col * r);
    }
    return mask;
}

// rmoutliers(A[, method][, percentiles-vec][, 'ThresholdFactor', tf]).
// Vectors: drop flagged ENTRIES (orientation preserved). Matrices:
// detect per column, remove any ROW containing an outlier. Optional
// 2nd output is the logical mask of removed entries (vector) / rows
// (matrix). DEEP-PROBE 2026-05-31: previously delegated to the default
// median detector and IGNORED method/percentiles/ThresholdFactor, and
// flattened matrices instead of removing rows.
void rmoutliers_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rmoutliers: requires at least 1 argument",
                    0, 0, "rmoutliers", "", "numkit:rmoutliers:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];

    auto lower = [](std::string s) {
        for (char &ch : s)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    };

    std::string method = "median";
    double loP = 0.0, hiP = 0.0;
    std::size_t ai = 1;
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
        const std::string m = lower(args[1].toString());
        if (m == "median" || m == "mean" || m == "quartiles") {
            method = m;
            ai = 2;
        } else if (m == "percentiles") {
            method = "percentiles";
            if (args.size() < 3 || args[2].numel() != 2)
                throw Error("rmoutliers: 'percentiles' requires a 2-element "
                            "[lower upper] vector",
                            0, 0, "rmoutliers", "", "numkit:rmoutliers:percentiles");
            loP = args[2].elemAsDouble(0);
            hiP = args[2].elemAsDouble(1);
            ai = 3;
        } else if (m == "grubbs" || m == "gesd" || m == "movmedian" || m == "movmean") {
            throw Error("rmoutliers: method '" + args[1].toString() +
                            "' is not supported in this revision "
                            "(median, mean, quartiles, percentiles only)",
                         0, 0, "rmoutliers", "", "numkit:rmoutliers:method");
        }
        // else: leave as a Name-Value name parsed below.
    }

    double userTf = (method == "quartiles") ? 1.5 : 3.0;
    for (std::size_t i = ai; i + 1 < args.size(); i += 2) {
        if (args[i].isChar() || args[i].isString()) {
            const std::string nm = lower(args[i].toString());
            if (nm == "thresholdfactor")
                userTf = args[i + 1].toScalar();
            else
                throw Error("rmoutliers: unknown option '" + args[i].toString() + "'",
                             0, 0, "rmoutliers", "", "numkit:rmoutliers:option");
        }
    }
    if (userTf < 0.0)
        throw Error("rmoutliers: ThresholdFactor must be nonnegative",
                     0, 0, "rmoutliers", "", "numkit:rmoutliers:tf");
    const double detectTf = (method == "quartiles") ? 2.0 * userTf : userTf;

    const std::size_t n = x.numel();
    if (n == 0) {
        outs[0] = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        if (nargout >= 2) outs[1] = Value::matrix(0, 0, ValueType::LOGICAL, mr);
        return;
    }
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    const double *xd = x.doubleData();
    const std::vector<uint8_t> mask = rmoutlierMask(x, method, loP, hiP, detectTf);

    if (r == 1 || c == 1) {
        // Vector: drop flagged entries, preserve orientation.
        ScratchArena scratch(mr);
        ScratchVec<double> kept(&scratch);
        kept.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            if (!mask[i]) kept.push_back(xd[i]);
        const bool colOrient = (r != 1);  // column vector → column output
        auto out = colOrient
            ? Value::matrix(kept.size(), 1, ValueType::DOUBLE, mr)
            : Value::matrix(1, kept.size(), ValueType::DOUBLE, mr);
        if (!kept.empty())
            std::copy(kept.begin(), kept.end(), out.doubleDataMut());
        outs[0] = out;
        if (nargout >= 2) {
            auto rm = Value::matrix(r, c, ValueType::LOGICAL, mr);
            uint8_t *rd = rm.logicalDataMut();
            for (std::size_t i = 0; i < n; ++i) rd[i] = mask[i];
            outs[1] = rm;
        }
    } else {
        // Matrix: remove any ROW with an outlier in any column.
        std::vector<uint8_t> rowRemove(r, 0);
        for (std::size_t col = 0; col < c; ++col)
            for (std::size_t i = 0; i < r; ++i)
                if (mask[col * r + i]) rowRemove[i] = 1;
        std::size_t keptRows = 0;
        for (std::size_t i = 0; i < r; ++i) if (!rowRemove[i]) ++keptRows;
        auto out = Value::matrix(keptRows, c, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        std::size_t orow = 0;
        for (std::size_t i = 0; i < r; ++i) {
            if (rowRemove[i]) continue;
            for (std::size_t col = 0; col < c; ++col)
                od[col * keptRows + orow] = xd[col * r + i];
            ++orow;
        }
        outs[0] = out;
        if (nargout >= 2) {
            auto rm = Value::matrix(r, 1, ValueType::LOGICAL, mr);
            uint8_t *rd = rm.logicalDataMut();
            for (std::size_t i = 0; i < r; ++i) rd[i] = rowRemove[i];
            outs[1] = rm;
        }
    }
}

void fillmissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fillmissing: requires (x, method[, constant_value][,'EndValues',ev])",
                    0, 0, "fillmissing", "", "numkit:fillmissing:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("fillmissing: method must be a string",
                    0, 0, "fillmissing", "", "numkit:fillmissing:method");

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return s;
    };
    const std::string m = lower(args[1].toString());
    auto *mr = ctx.engine->resource();

    // 'constant' takes a positional fill value (first non-string arg).
    double cv = 0.0;
    std::size_t ai = 2;
    if (m == "constant" && args.size() >= 3 &&
        !args[2].isChar() && !args[2].isString()) {
        cv = args[2].toScalar();
        ai = 3;
    }

    // Optional 'EndValues', ev name-value pair (extrap | none | nearest |
    // numeric constant). 'previous'/'next' EndValues deferred.
    FmEndMode endMode = FmEndMode::Extrap;
    double endVal = 0.0;
    bool haveEnd = false;
    for (std::size_t i = ai; i < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("fillmissing: expected an option name string",
                        0, 0, "fillmissing", "", "numkit:fillmissing:option");
        const std::string nm = lower(args[i].toString());
        if (i + 1 >= args.size())
            throw Error("fillmissing: option '" + nm + "' requires a value",
                        0, 0, "fillmissing", "", "numkit:fillmissing:option");
        if (nm == "endvalues") {
            haveEnd = true;
            const Value &ev = args[i + 1];
            if (ev.isChar() || ev.isString()) {
                const std::string evs = lower(ev.toString());
                if (evs == "extrap")       endMode = FmEndMode::Extrap;
                else if (evs == "none")    endMode = FmEndMode::None;
                else if (evs == "nearest") endMode = FmEndMode::Nearest;
                else if (evs == "previous" || evs == "next")
                    throw Error("fillmissing: EndValues '" + evs +
                                "' not supported in this revision ('extrap', "
                                "'none', 'nearest', or a numeric constant only)",
                                0, 0, "fillmissing", "", "numkit:fillmissing:endvalues");
                else
                    throw Error("fillmissing: unknown EndValues '" + evs + "'",
                                0, 0, "fillmissing", "", "numkit:fillmissing:endvalues");
            } else {
                endMode = FmEndMode::Const;
                endVal = ev.toScalar();
            }
        } else {
            throw Error("fillmissing: unknown option '" + nm + "'",
                        0, 0, "fillmissing", "", "numkit:fillmissing:option");
        }
    }

    if (haveEnd && endMode != FmEndMode::Extrap &&
        (m == "constant" || m == "mean" || m == "median"))
        throw Error("fillmissing: 'EndValues' is not supported with fill "
                    "method '" + m + "'",
                    0, 0, "fillmissing", "", "numkit:fillmissing:endvalues");

    outs[0] = fillmissing_of(args[0], m, cv, mr);

    if (haveEnd && endMode != FmEndMode::Extrap && args[0].numel() > 0) {
        const Value &x = args[0];
        const double *xd = x.doubleData();
        double *od = outs[0].doubleDataMut();
        const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
        const std::size_t c = (x.dims().ndim() >= 2)
                                ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
        if (r == 1) {
            apply_end_values_column(xd, od, x.numel(), endMode, endVal);
        } else {
            for (std::size_t col = 0; col < c; ++col)
                apply_end_values_column(xd + col * r, od + col * r, r, endMode, endVal);
        }
    }
}

void rmmissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rmmissing: requires at least 1 argument",
                    0, 0, "rmmissing", "", "numkit:rmmissing:nargin");
    outs[0] = rmmissing_of(args[0], ctx.engine->resource());
}

void standardizeMissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("standardizeMissing: requires (x, sentinel)",
                    0, 0, "standardizeMissing", "", "numkit:standardizeMissing:nargin");
    outs[0] = standardizeMissing_of(args[0], args[1].toScalar(), ctx.engine->resource());
}

// filloutliers(A, fillmethod[, findmethod][, NV])
//   fillmethod : numeric scalar | "center" | "clip" | "previous" |
//                "next" | "nearest" | "linear"
//   findmethod : "median" (default) | "mean" | "quartiles"
//   NV         : ThresholdFactor (default per-method)
void filloutliers_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filloutliers: requires (A, fillmethod[, findmethod][, NV])",
                    0, 0, "filloutliers", "", "numkit:filloutliers:nargin");
    auto lower = [](std::string v) {
        for (char &ch : v)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return v;
    };

    std::string detect = "median";
    double tf = 3.0;
    bool tf_set = false;
    double loP = 0.0, hiP = 0.0;
    std::size_t i = 2;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = lower(args[i].toString());
        // Distinguish findmethod string vs NV name. NV names are known.
        if (s == "thresholdfactor" || s == "maxnumoutliers" ||
            s == "samplepoints"    || s == "outlierlocations") {
            // fall through — handled by NV loop below.
        } else {
            detect = s;
            if (detect == "median" || detect == "mean" || detect == "quartiles") {
                ++i;
            } else if (detect == "percentiles") {
                ++i;
                if (i >= args.size() || args[i].numel() != 2)
                    throw Error("filloutliers: 'percentiles' requires a "
                                "2-element [lower upper] vector",
                                0, 0, "filloutliers", "",
                                "numkit:filloutliers:percentiles");
                loP = args[i].elemAsDouble(0);
                hiP = args[i].elemAsDouble(1);
                ++i;
            } else {
                throw Error("filloutliers: findmethod must be 'median', "
                            "'mean', 'quartiles', or 'percentiles' in this "
                            "revision (MATLAB also supports 'grubbs', 'gesd', "
                            "'movmedian', 'movmean' — deferred)",
                            0, 0, "filloutliers", "",
                            "numkit:filloutliers:findmethod");
            }
        }
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("filloutliers: expected name-value pair",
                        0, 0, "filloutliers", "", "numkit:filloutliers:nv");
        const std::string nm = lower(args[i].toString());
        if (nm == "thresholdfactor") {
            tf = args[i + 1].toScalar(); tf_set = true;
        } else {
            throw Error("filloutliers: unsupported name-value parameter '"
                        + args[i].toString() + "'",
                        0, 0, "filloutliers", "", "numkit:filloutliers:nv");
        }
        i += 2;
    }
    // MATLAB's per-method default ThresholdFactor.
    if (!tf_set) {
        if (detect == "quartiles") tf = 3.0;     // 1.5·IQR → scaled by 0.5 internally so 3.0 here
        else                       tf = 3.0;
    } else if (detect == "quartiles") {
        // User-set tf for quartiles means "k" in [Q1 - k·IQR, Q3 + k·IQR].
        // Our internal formula uses 0.5·tf·IQR, so multiply by 2.
        tf = 2.0 * tf;
    }
    outs[0] = filloutliers_of(args[0], args[1], detect, tf, loP, hiP,
                              ctx.engine->resource());
}

// ── range / mad / geomean / harmmean / moment / trimmean adapters ────

void range_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("range: requires at least 1 argument",
                    0, 0, "range", "", "numkit:range:nargin");
    const int dim = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = range_of(args[0], dim, ctx.engine->resource());
}

void mad_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mad: requires at least 1 argument",
                    0, 0, "mad", "", "numkit:mad:nargin");
    const int flag = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    const int dim  = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = mad_of(args[0], flag, dim, ctx.engine->resource());
}

// Parse a trailing 'omitnan'/'includenan' nanflag from a geomean/harmmean
// arg list. Returns the omit flag and the count of remaining numeric args.
bool parseMeanNanFlag(Span<const Value> args, const char *fn, std::size_t &nargs)
{
    nargs = args.size();
    if (nargs >= 2 && (args[nargs - 1].isChar() || args[nargs - 1].isString())) {
        std::string f = args[nargs - 1].toString();
        for (char &c : f) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (f == "omitnan")     { --nargs; return true; }
        if (f == "includenan")  { --nargs; return false; }
        throw Error(std::string(fn) + ": unknown option '" + f + "'",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badopt");
    }
    return false;
}

void geomean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("geomean: requires at least 1 argument",
                    0, 0, "geomean", "", "numkit:geomean:nargin");
    std::size_t nargs;
    const bool omitnan = parseMeanNanFlag(args, "geomean", nargs);
    const int dim = (nargs >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = geomean_of(args[0], dim, omitnan, ctx.engine->resource());
}

void harmmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("harmmean: requires at least 1 argument",
                    0, 0, "harmmean", "", "numkit:harmmean:nargin");
    std::size_t nargs;
    const bool omitnan = parseMeanNanFlag(args, "harmmean", nargs);
    const int dim = (nargs >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = harmmean_of(args[0], dim, omitnan, ctx.engine->resource());
}

void moment_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("moment: requires (x, order)",
                    0, 0, "moment", "", "numkit:moment:nargin");
    const int order = static_cast<int>(args[1].toScalar());
    const int dim   = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = moment_of(args[0], order, dim, ctx.engine->resource());
}

void trimmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("trimmean: requires (x, percent)",
                    0, 0, "trimmean", "", "numkit:trimmean:nargin");
    const double pct = args[1].toScalar();

    // trimmean(x, percent [, flag] [, dim]). The 3rd arg is EITHER a string
    // flag ('round' default, or 'floor') OR a numeric dim; if a flag is
    // present the dim may follow it. Distinguish by type before toScalar.
    bool useFloor = false;
    int dim = 0;
    std::size_t i = 2;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        std::string f = args[i].toString();
        for (char &c : f) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (f == "floor")      useFloor = true;
        else if (f == "round") useFloor = false;
        else
            throw Error("trimmean: flag must be 'round' or 'floor'",
                        0, 0, "trimmean", "", "numkit:trimmean:flag");
        ++i;
    }
    if (i < args.size())
        dim = static_cast<int>(args[i].toScalar());

    outs[0] = trimmean_of(args[0], pct, dim, useFloor, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
