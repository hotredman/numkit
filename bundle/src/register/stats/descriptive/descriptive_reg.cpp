// toolboxes/signal/src/descriptive/descriptive_reg.cpp
//
// CallContext register half of descriptive/descriptive.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/stats/distributions/normal.hpp>     // norminv for corrcoef conf bounds
#include <numkit/stats/distributions/students_t.hpp> // tcdf for corrcoef p-values
#include <numkit/stats/nan_aware/nan_aware.hpp>  // var_reg / std_reg / median_reg dispatch into stats:: when 'omitnan' is given
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "descriptive/descriptive_detail.hpp"
#include <numkit/ops/helpers.hpp>
#include "math/arithmetic/var_reduction.hpp"
#include <numkit/ops/reductions.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

// Helper: weighted variance / std on a flat 1-D array.
//   normFlag = 0  → divide by Σw - 0 (default for scalar w=0; sample)
//   normFlag = 1  → divide by Σw     (population; ML estimate)
//
// MATLAB's documented denominator for weighted variance is Σw (the
// "default" for vector weights — equivalent to normFlag=1 semantics).
// var(A, W, ...) with vector W therefore implies normFlag=1.
double weightedVarFlat(const double *x, const double *w, size_t n,
                       bool sqrtIt, bool omitNan)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double sw = 0.0, sxw = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double xi = x[i], wi = w[i];
        if (omitNan && std::isnan(xi)) continue;
        if (wi < 0.0)
            throw Error("var/std: weights must be non-negative",
                        0, 0, "var/std", "", "numkit:varstd:negWeight");
        sw  += wi;
        sxw += wi * xi;
    }
    if (sw == 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double mean = sxw / sw;
    double ss = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double xi = x[i], wi = w[i];
        if (omitNan && std::isnan(xi)) continue;
        const double d = xi - mean;
        ss += wi * d * d;
    }
    const double v = ss / sw;
    return sqrtIt ? std::sqrt(v) : v;
}

// Helper: scalar-normFlag variance/std on a flat 1-D array.
double scalarVarFlat(const double *x, size_t n, int normFlag,
                     bool sqrtIt, bool omitNan)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double s = 0.0;
    size_t cnt = 0;
    for (size_t i = 0; i < n; ++i) {
        if (omitNan && std::isnan(x[i])) continue;
        s += x[i]; ++cnt;
    }
    if (cnt == 0) return std::numeric_limits<double>::quiet_NaN();
    if (cnt == 1) return (normFlag == 1) ? 0.0
                                         : std::numeric_limits<double>::quiet_NaN();
    const double mean = s / static_cast<double>(cnt);
    double ss = 0.0;
    for (size_t i = 0; i < n; ++i) {
        if (omitNan && std::isnan(x[i])) continue;
        const double d = x[i] - mean;
        ss += d * d;
    }
    const double denom = (normFlag == 1) ? static_cast<double>(cnt)
                                         : static_cast<double>(cnt - 1);
    const double v = ss / denom;
    return sqrtIt ? std::sqrt(v) : v;
}

// Convert a Value to a flat row (used for 'all' / vecdim full-flatten /
// weight-vector inputs). Skips no elements.
std::vector<double> flatten(const Value &x)
{
    std::vector<double> out(x.numel());
    for (size_t i = 0; i < x.numel(); ++i) out[i] = x.elemAsDouble(i);
    return out;
}

// Common var/std driver — handles every variant (scalar w + scalar dim,
// 'all', vecdim full-flatten, weight vector).
//
// `sqrtIt = true` makes this `std`.
Value varStdDispatch(Span<const Value> args, bool sqrtIt, const char *fn, std::pmr::memory_resource *mr)
{
    bool omitNan = false;
    size_t n = stripNanFlag(args, omitNan, fn);

    const Value &x = args[0];

    // Parse args[1] (w) and args[2] (dim/'all'/vecdim) into a normalised
    // shape: scalar normFlag (0 or 1), or weight vector + flatten flag.
    int normFlag = 0;
    bool isWeightVec = false;
    const Value *wVec = nullptr;

    if (n >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() == 1 && args[1].isNumeric()) {
            normFlag = static_cast<int>(args[1].toScalar());
            if (normFlag != 0 && normFlag != 1)
                throw Error(std::string(fn) + ": w must be 0 or 1, or a "
                            "weight vector",
                            0, 0, fn, "", std::string("numkit:") + fn + ":w");
        } else {
            isWeightVec = true;
            wVec = &args[1];
        }
    }

    int dim = 0;
    bool flattenAll = false;
    if (n >= 3 && !args[2].isEmpty()) {
        const Value &a = args[2];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "all") flattenAll = true;
            else throw Error(std::string(fn) + ": unknown dim flag '" + s + "'",
                              0, 0, fn, "", std::string("numkit:") + fn + ":dim");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim: only full-flatten coverage supported
            std::vector<int> dims;
            for (size_t i = 0; i < a.numel(); ++i)
                dims.push_back(static_cast<int>(a.elemAsDouble(i)));
            const int rank = x.dims().is3D() ? 3
                              : (x.dims().isVector() || x.isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error(std::string(fn) + ": vecdim entries out of range",
                                0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error(std::string(fn) + ": partial vecdim reduction is "
                            "not yet supported (only full-flatten vecdim)",
                            0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
            flattenAll = true;
        }
    }

    // ── Weighted-vector path ──────────────────────────────────────────
    if (isWeightVec) {
        const bool xIsVec = x.dims().isVector() || x.isScalar();
        if (flattenAll || (dim == 0 && xIsVec)) {
            // Vector input or 'all': flatten + run weightedVarFlat. The
            // weight length must match the element count.
            auto xv = flatten(x);
            auto wv = flatten(*wVec);
            if (xv.size() != wv.size())
                throw Error(std::string(fn) + ": weight vector length must "
                            "match number of elements",
                            0, 0, fn, "", std::string("numkit:") + fn + ":wlen");
            const double v = weightedVarFlat(xv.data(), wv.data(),
                                             xv.size(), sqrtIt, omitNan);
            return Value::scalar(v, mr);
        }
        // Matrix (or explicit dim): MATLAB applies the weight vector along
        // the operating dimension, computing one weighted variance per
        // slice. Weight length must equal size(x, dim). 2-D only (N-D
        // weighted reduction is deferred).
        if (x.dims().ndim() > 2)
            throw Error(std::string(fn) + ": weight vector along a dimension "
                        "is supported for 2-D inputs only (N-D deferred)",
                        0, 0, fn, "", std::string("numkit:") + fn + ":wND");
        const int d = (dim == 0) ? firstNonSingletonDim(x) : dim;
        if (d != 1 && d != 2)
            throw Error(std::string(fn) + ": dim out of range",
                        0, 0, fn, "", std::string("numkit:") + fn + ":dim");
        const size_t r = static_cast<size_t>(x.dims().dim(0));
        const size_t c = static_cast<size_t>(x.dims().dim(1));
        auto wv = flatten(*wVec);
        const size_t need = (d == 1) ? r : c;
        if (wv.size() != need)
            throw Error(std::string(fn) + ": weight vector length must match "
                        "the length of the operating dimension",
                        0, 0, fn, "", std::string("numkit:") + fn + ":wlen");
        if (d == 1) {
            auto out = Value::matrix(1, c, ValueType::DOUBLE, mr);
            double *od = out.doubleDataMut();
            std::vector<double> col(r);
            for (size_t j = 0; j < c; ++j) {
                for (size_t i = 0; i < r; ++i) col[i] = x.elemAsDouble(j * r + i);
                od[j] = weightedVarFlat(col.data(), wv.data(), r, sqrtIt, omitNan);
            }
            return out;
        }
        auto out = Value::matrix(r, 1, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        std::vector<double> row(c);
        for (size_t i = 0; i < r; ++i) {
            for (size_t j = 0; j < c; ++j) row[j] = x.elemAsDouble(j * r + i);
            od[i] = weightedVarFlat(row.data(), wv.data(), c, sqrtIt, omitNan);
        }
        return out;
    }

    // ── Flatten 'all' / vecdim path ───────────────────────────────────
    if (flattenAll) {
        auto xv = flatten(x);
        const double v = scalarVarFlat(xv.data(), xv.size(), normFlag,
                                       sqrtIt, omitNan);
        return Value::scalar(v, mr);
    }

    // ── Standard scalar-dim path ──────────────────────────────────────
    if (omitNan) {
        if (x.type() == ValueType::COMPLEX) {
            return varianceComplex(x, normFlag, dim, mr, sqrtIt, true);
        }
        Value r = sqrtIt
                    ? ::numkit::stats::nanstdev(x, normFlag, dim, mr)
                    : ::numkit::stats::nanvar  (x, normFlag, dim, mr);
        if (x.type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), mr);
        return r;
    }
    return sqrtIt ? stdev(x, normFlag, dim, mr) : var(x, normFlag, dim, mr);
}

void var_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("var: requires at least 1 argument",
                     0, 0, "var", "", "numkit:var:nargin");
    outs[0] = varStdDispatch(args, /*sqrtIt=*/false, "var", ctx.engine->resource());
}

void std_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("std: requires at least 1 argument",
                     0, 0, "std", "", "numkit:std:nargin");
    outs[0] = varStdDispatch(args, /*sqrtIt=*/true, "std", ctx.engine->resource());
}

void median_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("median: requires at least 1 argument",
                     0, 0, "median", "", "numkit:median:nargin");
    bool omitNan = false;
    size_t n = stripNanFlag(args, omitNan, "median");
    int dim = 0;
    bool isAll = false;
    if (n >= 2 && !args[1].isEmpty()) {
        const Value &a = args[1];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "all") isAll = true;
            else throw Error("median: unknown flag '" + s + "'",
                              0, 0, "median", "", "numkit:median:badFlag");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim — full-flatten only
            const int rank = args[0].dims().is3D() ? 3
                              : (args[0].dims().isVector() || args[0].isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (size_t i = 0; i < a.numel(); ++i) {
                int d = static_cast<int>(a.elemAsDouble(i));
                if (d < 1 || d > rank)
                    throw Error("median: vecdim entries out of range",
                                0, 0, "median", "", "numkit:median:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("median: partial vecdim reduction not supported",
                            0, 0, "median", "", "numkit:median:vecdim");
            isAll = true;
        }
    }
    if (isAll) {
        // 'all' → flatten + median over all elements (skipping NaN if omitnan).
        if (args[0].type() == ValueType::COMPLEX) {
            const size_t total = args[0].numel();
            ScratchArena scratch(ctx.engine->resource());
            auto buf = ScratchVec<Complex>(total, &scratch);
            const Complex *src = args[0].complexData();
            size_t k = 0;
            for (size_t i = 0; i < total; ++i) {
                if (omitNan && isComplexNaNStats(src[i])) continue;
                buf[k++] = src[i];
            }
            outs[0] = Value::complexScalar(complexMedianFromSlice(buf.data(), k),
                                           ctx.engine->resource());
            return;
        }
        const size_t total = args[0].numel();
        ScratchArena scratch(ctx.engine->resource());
        auto buf = ScratchVec<double>(total, &scratch);
        const bool fastDouble = (args[0].type() == ValueType::DOUBLE);
        if (fastDouble)
            std::copy(args[0].doubleData(), args[0].doubleData() + total, buf.data());
        else
            for (size_t i = 0; i < total; ++i) buf[i] = args[0].elemAsDouble(i);
        size_t k = total;
        if (omitNan) k = compactNonNan(buf.data(), total);
        double v = medianFromSlice(buf.data(), k);
        Value r = Value::scalar(v, ctx.engine->resource());
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), ctx.engine->resource());
        outs[0] = std::move(r);
        return;
    }
    if (omitNan) {
        rejectComplexOmitNan(args[0], "median");
        Value r = ::numkit::stats::nanmedian(args[0], dim, ctx.engine->resource());
        if (args[0].type() == ValueType::SINGLE)
            r = narrowToSingle(std::move(r), ctx.engine->resource());
        outs[0] = std::move(r);
        return;
    }
    outs[0] = median(args[0], dim, ctx.engine->resource());
}

// Common parser for the trailing args of quantile/prctile:
//   fn(X, p[, dim] [, Method=method])
//
// Handles:
//   * 'all' (string) — flatten input
//   * scalar dim
//   * vecdim — only [1 2] / [1 2 3] / etc. covering ALL dims is supported
//     as a synonym for 'all'; partial reductions throw a documented error.
//   * Method N-V pair: 'midpoint' (default) | 'inclusive' | 'exclusive'
//                       | 'approximate' (falls back to midpoint).
struct QArgs {
    int dim = 0;
    bool flatten = false;
    QMethod method = QMethod::Midpoint;
};

QArgs parseQArgs(Span<const Value> args, size_t start, const Value &x,
                 const char *fn)
{
    QArgs q;
    size_t i = start;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        if (args[i].numel() == 1) {
            q.dim = static_cast<int>(args[i].toScalar());
        } else {
            // Vecdim: covers-all-dims → flatten; otherwise error.
            std::vector<int> dims;
            for (size_t j = 0; j < args[i].numel(); ++j)
                dims.push_back(static_cast<int>(args[i].elemAsDouble(j)));
            const int rank = x.dims().is3D() ? 3
                              : (x.dims().isVector() || x.isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error(std::string(fn) + ": vecdim entries out of range",
                                0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error(std::string(fn) + ": partial vecdim reduction is "
                            "not yet supported in numkit (only full-flatten "
                            "vecdim like [1 2] or 'all')",
                            0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
            q.flatten = true;
        }
        ++i;
    } else if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        std::string s = args[i].toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s == "all") {
            q.flatten = true;
            ++i;
        }
        // else: leave for the Name-Value loop below ("Method=...")
    }

    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error(std::string(fn) + ": expected Name-Value pair",
                        0, 0, fn, "", std::string("numkit:") + fn + ":nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (name == "method") {
            if (!args[i + 1].isChar() && !args[i + 1].isString())
                throw Error(std::string(fn) + ": Method must be a string",
                            0, 0, fn, "", std::string("numkit:") + fn + ":method");
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            // MATLAB's documented values are 'exact' (default) and
            // 'approximate'. 'exact' is the linear-interpolation order-
            // statistic method, which is numkit's Midpoint. The
            // midpoint/inclusive/exclusive names are kept for compatibility.
            if      (m == "exact")       q.method = QMethod::Midpoint;
            else if (m == "midpoint")    q.method = QMethod::Midpoint;
            else if (m == "inclusive")   q.method = QMethod::Inclusive;
            else if (m == "exclusive")   q.method = QMethod::Exclusive;
            else if (m == "approximate") q.method = QMethod::Approximate;
            else
                throw Error(std::string(fn) + ": Method must be 'exact' or "
                            "'approximate'",
                            0, 0, fn, "", std::string("numkit:") + fn + ":method");
        } else {
            throw Error(std::string(fn) + ": unknown Name-Value '" + name + "'",
                        0, 0, fn, "", std::string("numkit:") + fn + ":nv");
        }
        i += 2;
    }
    return q;
}

void quantile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("quantile: requires (X, p[, dim] [, Method=method])",
                     0, 0, "quantile", "", "numkit:quantile:nargin");
    auto q = parseQArgs(args, 2, args[0], "quantile");
    outs[0] = quantileWithOpts(args[0], args[1], q.dim, q.flatten, q.method, 1.0, "quantile", ctx.engine->resource());
}

void prctile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("prctile: requires (X, p[, dim] [, Method=method])",
                     0, 0, "prctile", "", "numkit:prctile:nargin");
    auto q = parseQArgs(args, 2, args[0], "prctile");
    outs[0] = quantileWithOpts(args[0], args[1], q.dim, q.flatten, q.method, 0.01, "prctile", ctx.engine->resource());
}

// 3rd mode output C: a cell array of the modal values. Each cell holds a
// column vector of the values that attain the modal frequency in that
// slice, sorted ascending; MATLAB ignores NaN. Supported for a real DOUBLE
// vector, 2-D matrix, or 'all'-flattened input.
Value computeModeCell(const Value &x, int dim, bool flatten,
                      std::pmr::memory_resource *mr)
{
    if (x.type() != ValueType::DOUBLE || x.dims().is3D() || x.dims().ndim() > 2)
        throw Error("mode: the 3rd output C is currently supported only for a "
                    "real double vector or 2-D matrix input",
                    0, 0, "mode", "", "numkit:mode:cellNd");
    auto tiedCol = [&](const double *s, size_t stride, size_t n) -> Value {
        std::vector<double> v;
        v.reserve(n);
        for (size_t k = 0; k < n; ++k) {
            const double d = s[k * stride];
            if (!std::isnan(d)) v.push_back(d);
        }
        std::sort(v.begin(), v.end());
        const size_t N = v.size();
        size_t maxc = 0;
        for (size_t i = 0; i < N;) {
            size_t j = i;
            while (j < N && v[j] == v[i]) ++j;
            if (j - i > maxc) maxc = j - i;
            i = j;
        }
        std::vector<double> tied;
        for (size_t i = 0; i < N;) {
            size_t j = i;
            while (j < N && v[j] == v[i]) ++j;
            if (j - i == maxc) tied.push_back(v[i]);
            i = j;
        }
        Value col = Value::matrix(tied.size(), 1, ValueType::DOUBLE, mr);
        for (size_t k = 0; k < tied.size(); ++k) col.doubleDataMut()[k] = tied[k];
        return col;
    };
    const double *data = x.numel() ? x.doubleData() : nullptr;
    if (flatten || x.isScalar() || x.dims().isVector()) {
        Value cell = Value::cell(1, 1, mr);
        cell.cellAt(0) = tiedCol(data, 1, x.numel());
        return cell;
    }
    const size_t R = x.dims().rows(), Cn = x.dims().cols();
    const int rdim = (dim == 0) ? 1 : dim;
    if (rdim == 1) {
        Value cell = Value::cell(1, Cn, mr);
        for (size_t c = 0; c < Cn; ++c) cell.cellAt(c) = tiedCol(data + c * R, 1, R);
        return cell;
    }
    Value cell = Value::cell(R, 1, mr);
    for (size_t r = 0; r < R; ++r) cell.cellAt(r) = tiedCol(data + r, R, Cn);
    return cell;
}

void mode_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("mode: requires at least 1 argument",
                     0, 0, "mode", "", "numkit:mode:nargin");
    int dim = 0;
    bool flatten = false;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &a = args[1];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (s == "all") flatten = true;
            else throw Error("mode: unknown flag '" + s + "'",
                             0, 0, "mode", "", "numkit:mode:badFlag");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim — full-flatten only
            const int rank = args[0].dims().is3D() ? 3
                              : (args[0].dims().isVector() || args[0].isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (size_t i = 0; i < a.numel(); ++i) {
                int d = static_cast<int>(a.elemAsDouble(i));
                if (d < 1 || d > rank)
                    throw Error("mode: vecdim entries out of range",
                                0, 0, "mode", "", "numkit:mode:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("mode: partial vecdim reduction not yet supported",
                            0, 0, "mode", "", "numkit:mode:vecdim");
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
        auto [v, c] = mode(flat, 2, mr);
        outs[0] = std::move(v);
        if (nargout > 1) outs[1] = std::move(c);
        if (nargout > 2) outs[2] = computeModeCell(args[0], 0, true, mr);
        return;
    }
    auto [v, c] = mode(args[0], dim, mr);
    outs[0] = std::move(v);
    if (nargout > 1)
        outs[1] = std::move(c);
    if (nargout > 2)
        outs[2] = computeModeCell(args[0], dim, false, mr);
}

// skewness_reg / kurtosis_reg moved to toolboxes/stats/src/moments/moments.cpp

void cov_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("cov: requires at least 1 argument",
                     0, 0, "cov", "", "numkit:cov:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();

    // A trailing NaN-policy string ('includenan'|'omitrows'|'partialrows')
    // may follow any signature. Strip it; the remaining args are numeric.
    CovNan nanMode = CovNan::Include;
    std::size_t nargs = args.size();
    if (nargs >= 2 && parseCovNanFlag(args[nargs - 1], nanMode))
        --nargs;

    if (nanMode != CovNan::Include) {
        // 'omitrows' / 'partialrows' over the numeric arg(s).
        //   1 numeric arg : cov(X, flag)
        //   2 numeric args: cov(X, w, flag) [w scalar 0/1] | cov(x, y, flag)
        //   3 numeric args: cov(x, y, w, flag)
        if (nargs == 1) {
            outs[0] = covNanAware(args[0], 0, nanMode, mr);
            return;
        }
        if (nargs == 2) {
            if (args[1].isScalar()) {
                const double v = args[1].toScalar();
                if (v == 0.0 || v == 1.0) {
                    outs[0] = covNanAware(args[0], static_cast<int>(v),
                                          nanMode, mr);
                    return;
                }
            }
            outs[0] = covNanAware(assembleXY(args[0], args[1], mr), 0,
                                  nanMode, mr);
            return;
        }
        const int w = static_cast<int>(args[2].toScalar());
        outs[0] = covNanAware(assembleXY(args[0], args[1], mr), w, nanMode, mr);
        return;
    }

    // 'includenan' (default) → plain cov over the numeric args.
    if (nargs == 1) {
        outs[0] = cov(args[0], 0, mr);
        return;
    }
    // 2-arg form is ambiguous: cov(x, normFlag) vs cov(x, y).
    // Disambiguate exactly the way MATLAB does: if the second arg is a
    // scalar (0 or 1), it's normFlag; otherwise it's y.
    if (nargs == 2) {
        if (args[1].isScalar()) {
            const double v = args[1].toScalar();
            if (v == 0.0 || v == 1.0) {
                outs[0] = cov(args[0], static_cast<int>(v), mr);
                return;
            }
        }
        outs[0] = cov(args[0], args[1], 0, mr);
        return;
    }
    // 3-arg form: cov(x, y, normFlag).
    const int w = static_cast<int>(args[2].toScalar());
    outs[0] = cov(args[0], args[1], w, mr);
}

void corrcoef_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("corrcoef: requires at least 1 argument",
                     0, 0, "corrcoef", "", "numkit:corrcoef:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();

    const bool twoArg =
        (args.size() >= 2 && !args[1].isChar() && !args[1].isString());
    const std::size_t nvStart = twoArg ? 2 : 1;
    const CorrcoefRows rows = parseCorrcoefRows(args, nvStart);
    const double alpha = parseCorrcoefAlpha(args, nvStart);

    if (rows == CorrcoefRows::All) {
        double n;
        if (!twoArg) {
            const Value &x = args[0];
            n = (x.dims().isVector() || x.isScalar())
                    ? static_cast<double>(x.numel())          // single variable
                    : static_cast<double>(x.dims().rows());   // n obs × p vars
            outs[0] = corrcoef(x, mr);
        } else {
            n = static_cast<double>(args[0].numel());         // corrcoef(x, y)
            outs[0] = corrcoef(args[0], args[1], mr);
        }
        if (nargout >= 2)
            outs[1] = corrcoefPValues(outs[0], n, mr);
        if (nargout >= 3) {
            Value RL, RU;
            corrcoefConfBounds(outs[0], n, alpha, RL, RU, mr);
            outs[2] = std::move(RL);
            if (nargout >= 4) outs[3] = std::move(RU);
        }
        return;
    }

    // NaN-aware paths. Assemble the working matrix ([x(:) y(:)] for the
    // two-vector form) and apply listwise/pairwise deletion.
    const Value M = twoArg ? assembleXY(args[0], args[1], mr) : args[0];
    const bool isVec = M.dims().isVector() || M.isScalar();

    if (rows == CorrcoefRows::Pairwise) {
        if (nargout >= 2)
            throw Error("corrcoef: [R, P] with 'Rows','pairwise' is not yet "
                        "supported (per-pair degrees of freedom)",
                        0, 0, "corrcoef", "", "numkit:corrcoef:PairwisePValue");
        outs[0] = isVec ? corrcoefScalarOne(mr) : corrcoefPairwise(M, mr);
        return;
    }

    // Complete (listwise deletion).
    if (isVec) {
        outs[0] = corrcoefScalarOne(mr);
        if (nargout >= 2) outs[1] = corrcoefScalarOne(mr);
        return;
    }
    outs[0] = corrcoefFromCov(covNanAware(M, 0, CovNan::Omitrows, mr), mr);
    if (nargout >= 2) {
        const double n = static_cast<double>(countCompleteRows(M));
        outs[1] = corrcoefPValues(outs[0], n, mr);
        if (nargout >= 3) {
            Value RL, RU;
            corrcoefConfBounds(outs[0], n, alpha, RL, RU, mr);
            outs[2] = std::move(RL);
            if (nargout >= 4) outs[3] = std::move(RU);
        }
    }
}

// nan*_reg adapters all moved to toolboxes/stats/src/nan_aware/nan_aware.cpp.

} // namespace detail

} // namespace numkit::stats
