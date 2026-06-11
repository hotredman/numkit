// toolboxes/signal/src/math/discrete/discrete_reg.cpp
//
// CallContext register half of math/discrete/discrete.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/math/discrete/discrete.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "discrete/discrete_detail.hpp"
#include <numkit/ops/helpers.hpp>
#include "rows_helpers.hpp"  // detail::collectRowsByIndex
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

namespace numkit::builtin {
using namespace numkit::math;  // C4c localized (umbrella removed)

namespace detail {

// Reshape a 1-D result to the requested orientation (column or row),
// preserving element order and type. MATLAB's unique returns ia/ic always as
// column vectors and the unique values matching the input orientation.
static Value orientUniqueVec(const Value &v, bool column,
                             std::pmr::memory_resource *mr)
{
    const size_t k = v.numel();
    if (k == 0) return v; // leave empties untouched
    const bool isCol = (v.dims().cols() == 1 && v.dims().rows() == k);
    if (column == isCol) return v; // already in the desired orientation
    Value out = Value::matrix(column ? k : 1, column ? 1 : k, v.type(), mr);
    if (v.type() == ValueType::COMPLEX)
        std::copy(v.complexData(), v.complexData() + k, out.complexDataMut());
    else
        std::copy(v.doubleData(), v.doubleData() + k, out.doubleDataMut());
    return out;
}

// Narrow a DOUBLE unique result (values or 'rows' matrix) back to the input's
// class. MATLAB's unique preserves the class on the VALUES (the ia/ic indices
// stay double) — same rule as sort. `d` is always double here (the unique
// machinery runs on a promoted copy), so narrowing always round-trips exactly
// for char codes / logical 0-1 / in-range integers. bugs/builtin/unique-typeclass.md.
static Value narrowUniqueClass(const Value &d, ValueType vt,
                               std::pmr::memory_resource *mr)
{
    if (vt == ValueType::LOGICAL) {
        Value r = createForDims(d.dims(), ValueType::LOGICAL, mr);
        uint8_t *dst = r.logicalDataMut();
        const size_t n = d.numel();
        for (size_t i = 0; i < n; ++i) dst[i] = (d.elemAsDouble(i) != 0.0) ? 1 : 0;
        return r;
    }
    if (vt == ValueType::CHAR) {
        Value r = createForDims(d.dims(), ValueType::CHAR, mr);
        char *dst = r.charDataMut();
        const size_t n = d.numel();
        for (size_t i = 0; i < n; ++i)
            dst[i] = static_cast<char>(static_cast<int>(d.elemAsDouble(i)));
        return r;
    }
    return doubleToIntegerExact(d, vt, mr);   // INT8..UINT64
}

// ── setops type-class helpers (ismember/intersect/setdiff/union) ──────────
// The setop machinery is double-only. Promote a char/logical/integer operand
// to double; leave double/complex untouched. bugs/builtin/setops-typeclass.md.
static bool setopNarrowable(ValueType t)
{
    return isIntegerType(t) || t == ValueType::LOGICAL || t == ValueType::CHAR;
}
static Value setopPromote(const Value &v, std::pmr::memory_resource *mr)
{
    return setopNarrowable(v.type()) ? toDoubleValue(v, mr) : v;
}
// The class to narrow the VALUES output back to (intersect/setdiff/union
// preserve the input class). Only when BOTH operands share the same
// char/logical/integer class; mixed classes or any double => DOUBLE (no narrow,
// matching MATLAB's promote-to-double for mixed types).
static ValueType setopNarrowClass(const Value &a, const Value &b)
{
    if (a.type() == b.type() && setopNarrowable(a.type())) return a.type();
    return ValueType::DOUBLE;
}

void unique_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("unique: requires 1 argument",
                     0, 0, "unique", "", "numkit:unique:nargin");
    auto *mr = ctx.engine->resource();

    // MATLAB unique preserves the input class on the VALUES (integer / logical
    // / char) while ia/ic stay double — but the unique machinery below is
    // double-only and throws "Not a double array" otherwise. Promote a
    // non-double/non-complex input to a double working copy, run unique on it,
    // then narrow outs[0] back to the class. bugs/builtin/unique-typeclass.md.
    const ValueType origType = args[0].type();
    const bool needsNarrow = isIntegerType(origType)
                          || origType == ValueType::LOGICAL
                          || origType == ValueType::CHAR;
    Value xd;
    if (needsNarrow) xd = toDoubleValue(args[0], mr);
    const Value &x = needsNarrow ? xd : args[0];
    auto narrowVals = [&](Value v) -> Value {
        return needsNarrow ? narrowUniqueClass(v, origType, mr) : v;
    };

    // Unique values are a row vector only when the input is a row vector;
    // a column vector or matrix input produces a column.
    const bool cIsRow =
        !x.dims().is3D() && x.dims().rows() == 1;

    bool useRows = false;
    bool stable  = false;
    bool last    = false;   // 'last': ia selects the LAST occurrence (sorted)
    for (size_t i = 1; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.type() != ValueType::CHAR)
            throw Error("unique: extra arguments must be string flags",
                         0, 0, "unique", "", "numkit:unique:badArg");
        std::string s = a.toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s == "rows") useRows = true;
        else if (s == "stable") stable = true;
        else if (s == "sorted") stable = false;
        else if (s == "first") last = false;
        else if (s == "last")  last = true;
        else {
            throw Error("unique: unknown flag '" + s + "'",
                         0, 0, "unique", "", "numkit:unique:badFlag");
        }
    }
    // NOTE: 'last' is honoured for the default (sorted) setOrder across the
    // vector, complex and 'rows' paths. With 'stable' the occurrence ORDER
    // (MATLAB orders by last occurrence) is not yet matched — 'stable' wins
    // and 'last' is ignored there (rare combo; see bugs/builtin/unique-last.md).

    if (useRows) {
        // C is a matrix of unique rows; ia/ic are column vectors. 'stable'
        // keeps rows in first-occurrence order (default 'sorted').
        if (nargout <= 1) { outs[0] = narrowVals(uniqueRows(x, mr, stable)); return; }
        auto [c, ia, ic] = uniqueRowsWithIndices(x, mr, stable, last);
        outs[0] = narrowVals(std::move(c));
        if (nargout > 1) outs[1] = orientUniqueVec(ia, /*column=*/true, mr);
        if (nargout > 2) outs[2] = orientUniqueVec(ic, /*column=*/true, mr);
        return;
    }

    if (nargout <= 1) {
        outs[0] = narrowVals(orientUniqueVec(unique(x, mr, stable), !cIsRow, mr));
        return;
    }
    auto [c, ia, ic] = uniqueWithIndices(x, mr, stable, last);
    outs[0] = narrowVals(orientUniqueVec(c, !cIsRow, mr));
    if (nargout > 1) outs[1] = orientUniqueVec(ia, /*column=*/true, mr);
    if (nargout > 2) outs[2] = orientUniqueVec(ic, /*column=*/true, mr);
}

#define NK_BIN_SETOP_REG(name, fn)                                             \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.size() < 2)                                                   \
            throw Error(#name ": requires 2 arguments",                       \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        outs[0] = fn(args[0], args[1], ctx.engine->resource());               \
    }

// histcounts(x, edges[, name, value...]): bin counts, optionally normalized.
// Edges may be passed positionally (histcounts(x, edges)) or via the
// 'BinEdges' name-value pair (histcounts(x, 'BinEdges', edges)). The second
// output returns the bin edges as a row vector: [n, e] = histcounts(...).
// 'Normalization' mode ∈ {count, probability, countdensity, pdf, cumcount,
// cdf}. Automatic binning (nbins / 'BinWidth' / 'BinLimits' / 'BinMethod')
// is not supported — edges must be given explicitly.
void histcounts_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("histcounts: requires at least 2 arguments",
                     0, 0, "histcounts", "", "numkit:histcounts:nargin");
    auto *mr = ctx.engine->resource();

    HistNorm norm = HistNorm::Count;
    Value edges = Value::Empty;
    bool haveEdges = false;
    bool binMethodIntegers = false;

    // A non-char second argument is the positional edges vector; otherwise
    // every trailing argument is a name-value pair (incl. 'BinEdges').
    size_t optStart = 1;
    if (args[1].type() != ValueType::CHAR) {
        edges = args[1];
        haveEdges = true;
        optStart = 2;
    }

    for (size_t i = optStart; i + 1 < args.size(); ++i) {
        if (args[i].type() != ValueType::CHAR) continue;
        std::string key = args[i].toString();
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (key == "normalization") {
            if (args[i + 1].type() != ValueType::CHAR)
                throw Error("histcounts: 'Normalization' value must be a string",
                             0, 0, "histcounts", "", "numkit:histcounts:badNorm");
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if      (m == "count")        norm = HistNorm::Count;
            else if (m == "probability")  norm = HistNorm::Probability;
            else if (m == "countdensity") norm = HistNorm::CountDensity;
            else if (m == "pdf")          norm = HistNorm::Pdf;
            else if (m == "cumcount")     norm = HistNorm::CumCount;
            else if (m == "cdf")          norm = HistNorm::Cdf;
            else
                throw Error("histcounts: unknown Normalization '" + m + "'",
                             0, 0, "histcounts", "", "numkit:histcounts:badNorm");
            ++i;   // consume the value
        } else if (key == "binedges") {
            edges = args[i + 1];
            haveEdges = true;
            ++i;   // consume the value
        } else if (key == "binmethod") {
            if (args[i + 1].type() != ValueType::CHAR)
                throw Error("histcounts: 'BinMethod' value must be a string",
                             0, 0, "histcounts", "", "numkit:histcounts:badBinMethod");
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (m == "integers")
                binMethodIntegers = true;
            else
                throw Error("histcounts: 'BinMethod' '" + m + "' not supported "
                                "(only 'integers'; use explicit edges otherwise)",
                             0, 0, "histcounts", "", "numkit:histcounts:badBinMethod");
            ++i;   // consume the value
        } else {
            throw Error("histcounts: option '" + args[i].toString() +
                            "' not supported (use explicit edges or 'BinEdges')",
                         0, 0, "histcounts", "", "numkit:histcounts:badOption");
        }
    }

    // 'BinMethod','integers': one unit-width bin centered on each integer in
    // [round(min), round(max)] of the finite data; bin edges are center +/-0.5.
    // (MATLAB caps the integers method at 65536 bins and then widens; that
    // widening is deferred — the common small-range case is exact.)
    if (binMethodIntegers && !haveEdges) {
        const Value &x = args[0];
        const size_t nx = x.numel();
        double lo = 0.0, hi = 0.0;
        bool any = false;
        for (size_t k = 0; k < nx; ++k) {
            const double v = x.elemAsDouble(k);
            if (!std::isfinite(v)) continue;
            if (!any) { lo = hi = v; any = true; }
            else { if (v < lo) lo = v; if (v > hi) hi = v; }
        }
        long first = any ? static_cast<long>(std::llround(lo)) : 0;
        long last  = any ? static_cast<long>(std::llround(hi)) : 0;
        if (last < first) last = first;
        const size_t nEdges = static_cast<size_t>(last - first) + 2;
        edges = Value::matrix(1, nEdges, ValueType::DOUBLE, mr);
        double *ed = edges.doubleDataMut();
        for (size_t e = 0; e < nEdges; ++e)
            ed[e] = (static_cast<double>(first) - 0.5) + static_cast<double>(e);
        haveEdges = true;
    }

    if (!haveEdges)
        throw Error("histcounts: bin edges required — automatic binning "
                     "(nbins / 'BinWidth' / 'BinLimits') is not supported",
                     0, 0, "histcounts", "", "numkit:histcounts:noEdges");

    outs[0] = histcounts(args[0], edges, norm, mr);

    // [n, edges] = histcounts(...): return the edges as a row vector.
    if (nargout >= 2) {
        validateEdges(edges, "histcounts");
        const size_t ne = edges.numel();
        auto e = Value::matrix(1, ne, ValueType::DOUBLE, mr);
        const double *src = edges.doubleData();
        std::copy(src, src + ne, e.doubleDataMut());
        outs[1] = e;
    }
}

// histc(x, edges): legacy bin counts (length(edges) bins, last = exact
// equal to edges(end)). [n, bin] = histc(...) also returns the 1-based bin
// index of each element (0 if out of range), same shape as x.
void histc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("histc: requires (x, edges)",
                     0, 0, "histc", "", "numkit:histc:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = histc(args[0], args[1], mr);
    if (nargout > 1) {
        const Value &x = args[0], &edges = args[1];
        const std::size_t nE = edges.numel();
        const double *e = edges.doubleData();
        Value binOut = createLike(x, ValueType::DOUBLE, mr);
        double *bd = binOut.doubleDataMut();
        const double *p = x.doubleData();
        const std::size_t n = x.numel();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t b = histcBin(p[i], e, nE);
            bd[i] = (b == SIZE_MAX) ? 0.0 : static_cast<double>(b + 1);
        }
        outs[1] = std::move(binOut);
    }
}

// ismember(A,B,'rows'): row-wise membership. tf(i) is true iff row i of A
// equals some row of B; loc(i) is the LOWEST 1-based B-row index (0 if absent).
// Both outputs are column vectors of height size(A,1). 2-D DOUBLE only; a row
// containing NaN never matches. vs MATLAB R2025b.
std::pair<Value, Value>
ismemberRows(const Value &A, const Value &B, bool wantLoc, std::pmr::memory_resource *mr)
{
    validateUniqueRowsInput(A, "ismember");
    validateUniqueRowsInput(B, "ismember");
    const size_t ar = A.dims().rows(), ac = A.dims().cols();
    const size_t br = B.dims().rows(), bc = B.dims().cols();
    if (ar > 0 && br > 0 && ac != bc)
        throw Error("ismember: 'rows' inputs must have the same number of columns",
                     0, 0, "ismember", "", "numkit:ismember:rowsCols");

    Value tf = Value::matrix(ar, 1, ValueType::LOGICAL, mr);
    uint8_t *tfd = (ar > 0) ? tf.logicalDataMut() : nullptr;
    Value loc;
    double *lo = nullptr;
    if (wantLoc) {
        loc = Value::matrix(ar, 1, ValueType::DOUBLE, mr);
        if (ar > 0) lo = loc.doubleDataMut();
    }
    if (ar == 0) return {std::move(tf), std::move(loc)};

    const double *ad = A.doubleData();
    ScratchArena scratch(mr);
    std::pmr::unordered_map<RowKey, double, RowKeyHash, RowKeyEq> bmap(&scratch);
    if (br > 0) {
        const double *bd = B.doubleData();
        for (size_t r = 0; r < br; ++r)
            if (!rowHasNan(bd, bc, br, r))
                bmap.try_emplace(extractRow(bd, bc, br, r, &scratch),
                                 static_cast<double>(r + 1));  // try_emplace keeps the lowest index
    }
    for (size_t r = 0; r < ar; ++r) {
        if (rowHasNan(ad, ac, ar, r)) {
            tfd[r] = 0;
            if (lo) lo[r] = 0.0;
            continue;
        }
        RowKey key = extractRow(ad, ac, ar, r, &scratch);
        auto it = bmap.find(key);
        const bool found = (it != bmap.end());
        tfd[r] = found ? 1 : 0;
        if (lo) lo[r] = found ? it->second : 0.0;
    }
    return {std::move(tf), std::move(loc)};
}

// ismember(a,b): tf membership mask; [tf,loc] = ismember(...) also returns
// loc(i) = the LOWEST 1-based index of a(i) in b (0 if absent), per MATLAB.
void ismember_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ismember: requires 2 arguments", 0, 0, "ismember", "", "numkit:ismember:nargin");
    auto *mr = ctx.engine->resource();
    // ismember accepts char/logical/integer operands; its outputs are already
    // logical (tf) + double (loc), so only the operands need promoting to
    // double (no value-narrow). bugs/builtin/setops-typeclass.md.
    Value A = setopPromote(args[0], mr), B = setopPromote(args[1], mr);
    // ismember(A,B,'rows'): each row is one element; outputs are columns.
    {
        bool rows = false;
        for (size_t i = 2; i < args.size(); ++i)
            if (args[i].isChar() || args[i].isString()) {
                std::string s = args[i].toString();
                for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (s == "rows") rows = true;
            }
        if (rows) {
            auto [tf, loc] = ismemberRows(A, B, /*wantLoc=*/nargout > 1, mr);
            outs[0] = std::move(tf);
            if (nargout > 1) outs[1] = std::move(loc);
            return;
        }
    }
    if (A.type() == ValueType::COMPLEX || B.type() == ValueType::COMPLEX) {
        auto [tf, loc] = ismemberComplex(A, B, /*wantLoc=*/nargout > 1, mr);
        outs[0] = std::move(tf);
        if (nargout > 1) outs[1] = std::move(loc);
        return;
    }
    outs[0] = ismember(A, B, mr);
    if (nargout > 1) {
        const Value &a = A, &b = B;
        ScratchArena scratch(mr);
        std::pmr::unordered_map<double, double, DoubleHashEq0> idxB(&scratch);
        idxB.reserve(b.numel());
        const double *pb = b.doubleData();
        for (size_t i = 0; i < b.numel(); ++i)
            if (!std::isnan(pb[i]))
                idxB.emplace(pb[i], static_cast<double>(i + 1));  // emplace keeps the lowest index
        Value loc = createLike(a, ValueType::DOUBLE, mr);
        double *lo = loc.doubleDataMut();
        const double *pa = a.doubleData();
        for (size_t i = 0; i < a.numel(); ++i) {
            const double v = pa[i];
            auto it = std::isnan(v) ? idxB.end() : idxB.find(v);
            lo[i] = (it != idxB.end()) ? it->second : 0.0;
        }
        outs[1] = std::move(loc);
    }
}

// union / intersect / setdiff accept a trailing 'sorted' (default) or
// 'stable' setOrder flag; 'stable' keeps first-occurrence (A-then-B) order.
namespace {
bool wantsStable(Span<const Value> args, size_t start)
{
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string s = args[i].toString();
            for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (s == "stable") return true;
        }
    }
    return false;
}

// ── set-operation index outputs (ia / ib) ──────────────────────────────
enum class SetOpKind { Intersect, Setdiff, Union };

void buildFirstIndexMap(const Value &x,
                        std::pmr::unordered_map<double, size_t, DoubleHashEq0> &m)
{
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isnan(v)) m.try_emplace(v, i + 1); // 1-based first occurrence
    }
}

// 1-based index of value v in x (finite via map, NaN via linear scan); 0 if absent.
size_t setopFirstIndex(const Value &x,
                       const std::pmr::unordered_map<double, size_t, DoubleHashEq0> &m,
                       double v)
{
    if (std::isnan(v)) {
        const size_t n = x.numel();
        for (size_t i = 0; i < n; ++i)
            if (std::isnan(x.elemAsDouble(i))) return i + 1;
        return 0;
    }
    auto it = m.find(v);
    return it == m.end() ? 0 : it->second;
}

// Emit ia (and ib for intersect/union) for a set operation, matching MATLAB:
// the index vectors are always columns. intersect/setdiff: ia indexes A,
// ib indexes B; union: ia indexes the A-sourced result elements, ib the
// B-only ones.
void emitSetopIndices(SetOpKind kind, const Value &A, const Value &B,
                      const Value &result, size_t nargout, Span<Value> outs,
                      std::pmr::memory_resource *mr, const char *fn)
{
    if (A.type() == ValueType::COMPLEX || B.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": index outputs are not supported for "
                    "complex inputs", 0, 0, fn, "",
                    std::string("numkit:") + fn + ":complexIdx");

    ScratchArena scratch(mr);
    std::pmr::unordered_map<double, size_t, DoubleHashEq0> mapA(&scratch);
    buildFirstIndexMap(A, mapA);

    if (kind == SetOpKind::Union) {
        std::pmr::unordered_map<double, size_t, DoubleHashEq0> mapB(&scratch);
        buildFirstIndexMap(B, mapB);
        auto iaVec = ScratchVec<double>(&scratch);
        auto ibVec = ScratchVec<double>(&scratch);
        const size_t k = result.numel();
        for (size_t i = 0; i < k; ++i) {
            const double v = result.elemAsDouble(i);
            const size_t ai = setopFirstIndex(A, mapA, v);
            if (ai != 0) iaVec.push_back(static_cast<double>(ai));
            else ibVec.push_back(static_cast<double>(setopFirstIndex(B, mapB, v)));
        }
        auto colOf = [&](const ScratchVec<double> &v) {
            Value c = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
            if (!v.empty()) std::copy(v.begin(), v.end(), c.doubleDataMut());
            return c;
        };
        if (nargout >= 2) outs[1] = colOf(iaVec);
        if (nargout >= 3) outs[2] = colOf(ibVec);
        return;
    }

    const size_t k = result.numel();
    Value ia = Value::matrix(k, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < k; ++i)
        ia.doubleDataMut()[i] =
            static_cast<double>(setopFirstIndex(A, mapA, result.elemAsDouble(i)));
    if (nargout >= 2) outs[1] = ia;

    if (kind == SetOpKind::Intersect && nargout >= 3) {
        std::pmr::unordered_map<double, size_t, DoubleHashEq0> mapB(&scratch);
        buildFirstIndexMap(B, mapB);
        Value ib = Value::matrix(k, 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < k; ++i)
            ib.doubleDataMut()[i] =
                static_cast<double>(setopFirstIndex(B, mapB, result.elemAsDouble(i)));
        outs[2] = ib;
    }
}

// True if a trailing 'rows' flag is present (case-insensitive).
bool wantsRows(Span<const Value> args, size_t start)
{
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string s = args[i].toString();
            for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (s == "rows") return true;
        }
    }
    return false;
}

// Row-wise union/intersect/setdiff (MATLAB ..(A,B,'rows')). Treats each row
// as an element; the result is the sorted set of unique rows (NaN-containing
// rows are distinct and sort last, matching unique('rows')). 2-D DOUBLE only.
Value setOpRows(const Value &A, const Value &B, SetOpKind kind,
                const char *fn, std::pmr::memory_resource *mr)
{
    validateUniqueRowsInput(A, fn);
    validateUniqueRowsInput(B, fn);
    const size_t ar = A.dims().rows(), ac = A.dims().cols();
    const size_t br = B.dims().rows(), bc = B.dims().cols();
    if (ar > 0 && br > 0 && ac != bc)
        throw Error(std::string(fn) + ": 'rows' inputs must have the same "
                    "number of columns", 0, 0, fn, "",
                    std::string("numkit:") + fn + ":rowsCols");
    const size_t cols = (ar > 0) ? ac : bc;

    // union(A,B,'rows') == unique rows of the vertical concatenation [A; B].
    if (kind == SetOpKind::Union) {
        const size_t nr = ar + br;
        if (nr == 0) return emptyRowsResult(cols, mr);
        auto combined = Value::matrix(nr, cols, ValueType::DOUBLE, mr);
        double *cd = combined.doubleDataMut();
        const double *ad = (ar > 0) ? A.doubleData() : nullptr;
        const double *bd = (br > 0) ? B.doubleData() : nullptr;
        for (size_t c = 0; c < cols; ++c) {
            for (size_t r = 0; r < ar; ++r)      cd[c * nr + r]      = ad[c * ar + r];
            for (size_t r = 0; r < br; ++r)      cd[c * nr + ar + r] = bd[c * br + r];
        }
        return uniqueRows(combined, mr);
    }

    // intersect / setdiff: unique rows of A that ARE / ARE NOT present in B.
    const bool wantInB = (kind == SetOpKind::Intersect);
    if (ar == 0) return emptyRowsResult(cols, mr);
    const double *ad = A.doubleData();
    const double *bd = (br > 0) ? B.doubleData() : nullptr;

    ScratchArena scratch(mr);
    std::pmr::unordered_map<RowKey, char, RowKeyHash, RowKeyEq> bset(&scratch);
    for (size_t r = 0; r < br; ++r)
        if (!rowHasNan(bd, bc, br, r))
            bset.try_emplace(extractRow(bd, bc, br, r, &scratch), char{1});

    std::pmr::unordered_map<RowKey, char, RowKeyHash, RowKeyEq> seen(&scratch);
    auto nonNan = ScratchVec<size_t>(&scratch);
    auto nanIdx = ScratchVec<size_t>(&scratch);
    for (size_t r = 0; r < ar; ++r) {
        if (rowHasNan(ad, ac, ar, r)) {
            // NaN row: never equal to anything, so never "in B".
            if (!wantInB) nanIdx.push_back(r);  // setdiff keeps; intersect drops
            continue;
        }
        RowKey key = extractRow(ad, ac, ar, r, &scratch);
        if ((bset.count(key) > 0) != wantInB) continue;
        if (seen.try_emplace(std::move(key), char{1}).second) nonNan.push_back(r);
    }
    std::sort(nonNan.begin(), nonNan.end(),
              [ad, ac, ar](size_t a, size_t b) { return rowLexCmp(ad, ac, ar, a, b) < 0; });
    nonNan.insert(nonNan.end(), nanIdx.begin(), nanIdx.end());
    if (nonNan.empty()) return emptyRowsResult(cols, mr);
    return detail::collectRowsByIndex(mr, A, nonNan.data(), nonNan.size());
}

// Row-wise setxor (MATLAB setxor(A,B,'rows')): the symmetric difference of the
// row sets — unique rows present in exactly one of A or B, sorted. Computed on
// the vertical concatenation [A;B] so a single collectRowsByIndex selects the
// kept rows. A NaN-containing row is never equal to anything (NaN != NaN), so
// every NaN row counts as "only in its side" and is kept (appended last).
// 2-D DOUBLE only. vs MATLAB R2025b.
Value setxorRows(const Value &A, const Value &B, const char *fn,
                 std::pmr::memory_resource *mr)
{
    validateUniqueRowsInput(A, fn);
    validateUniqueRowsInput(B, fn);
    const size_t ar = A.dims().rows(), ac = A.dims().cols();
    const size_t br = B.dims().rows(), bc = B.dims().cols();
    if (ar > 0 && br > 0 && ac != bc)
        throw Error(std::string(fn) + ": 'rows' inputs must have the same "
                    "number of columns", 0, 0, fn, "",
                    std::string("numkit:") + fn + ":rowsCols");
    const size_t cols = (ar > 0) ? ac : bc;
    const size_t nr = ar + br;
    if (nr == 0) return emptyRowsResult(cols, mr);

    auto combined = Value::matrix(nr, cols, ValueType::DOUBLE, mr);
    double *cd = combined.doubleDataMut();
    const double *ad = (ar > 0) ? A.doubleData() : nullptr;
    const double *bd = (br > 0) ? B.doubleData() : nullptr;
    for (size_t c = 0; c < cols; ++c) {
        for (size_t r = 0; r < ar; ++r) cd[c * nr + r]      = ad[c * ar + r];
        for (size_t r = 0; r < br; ++r) cd[c * nr + ar + r] = bd[c * br + r];
    }
    const double *cdp = combined.doubleData();

    ScratchArena scratch(mr);
    std::pmr::unordered_map<RowKey, char, RowKeyHash, RowKeyEq> aset(&scratch), bset(&scratch);
    for (size_t r = 0; r < ar; ++r)
        if (!rowHasNan(cdp, cols, nr, r))
            aset.try_emplace(extractRow(cdp, cols, nr, r, &scratch), char{1});
    for (size_t r = 0; r < br; ++r)
        if (!rowHasNan(cdp, cols, nr, ar + r))
            bset.try_emplace(extractRow(cdp, cols, nr, ar + r, &scratch), char{1});

    std::pmr::unordered_map<RowKey, char, RowKeyHash, RowKeyEq> seen(&scratch);
    auto nonNan = ScratchVec<size_t>(&scratch);
    auto nanIdx = ScratchVec<size_t>(&scratch);
    for (size_t ci = 0; ci < nr; ++ci) {
        const bool fromA = (ci < ar);
        if (rowHasNan(cdp, cols, nr, ci)) { nanIdx.push_back(ci); continue; }
        RowKey key = extractRow(cdp, cols, nr, ci, &scratch);
        // keep iff present only in this row's own side
        const auto &other = fromA ? bset : aset;
        if (other.count(key) > 0) continue;
        if (seen.try_emplace(std::move(key), char{1}).second) nonNan.push_back(ci);
    }
    std::sort(nonNan.begin(), nonNan.end(),
              [cdp, cols, nr](size_t a, size_t b) { return rowLexCmp(cdp, cols, nr, a, b) < 0; });
    nonNan.insert(nonNan.end(), nanIdx.begin(), nanIdx.end());
    if (nonNan.empty()) return emptyRowsResult(cols, mr);
    return detail::collectRowsByIndex(mr, combined, nonNan.data(), nonNan.size());
}
} // namespace

void union_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("union: requires 2 arguments", 0, 0, "union", "", "numkit:union:nargin");
    auto *mr = ctx.engine->resource();
    // char/logical/integer operands: promote to double, compute, then narrow
    // the VALUES output back to the shared class (ia/ib stay double). The
    // narrow runs AFTER emitSetopIndices, which needs the double values.
    const ValueType setNarrow = setopNarrowClass(args[0], args[1]);
    Value A = setopPromote(args[0], mr), B = setopPromote(args[1], mr);
    auto narrowOut = [&]{ if (setNarrow != ValueType::DOUBLE)
                              outs[0] = narrowUniqueClass(outs[0], setNarrow, mr); };
    if (wantsRows(args, 2)) {
        if (nargout >= 2)
            throw Error("union: 'rows' index outputs (ia, ib) are not yet "
                        "supported in this revision", 0, 0, "union", "",
                        "numkit:union:rowsIdx");
        outs[0] = setOpRows(A, B, SetOpKind::Union, "union", mr);
        narrowOut();
        return;
    }
    outs[0] = setUnion(A, B, mr, wantsStable(args, 2));
    if (nargout >= 2)
        emitSetopIndices(SetOpKind::Union, A, B, outs[0], nargout, outs,
                         mr, "union");
    narrowOut();
}

void intersect_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("intersect: requires 2 arguments", 0, 0, "intersect", "", "numkit:intersect:nargin");
    auto *mr = ctx.engine->resource();
    const ValueType setNarrow = setopNarrowClass(args[0], args[1]);
    Value A = setopPromote(args[0], mr), B = setopPromote(args[1], mr);
    auto narrowOut = [&]{ if (setNarrow != ValueType::DOUBLE)
                              outs[0] = narrowUniqueClass(outs[0], setNarrow, mr); };
    if (wantsRows(args, 2)) {
        if (nargout >= 2)
            throw Error("intersect: 'rows' index outputs (ia, ib) are not yet "
                        "supported in this revision", 0, 0, "intersect", "",
                        "numkit:intersect:rowsIdx");
        outs[0] = setOpRows(A, B, SetOpKind::Intersect, "intersect", mr);
        narrowOut();
        return;
    }
    outs[0] = setIntersect(A, B, mr, wantsStable(args, 2));
    if (nargout >= 2)
        emitSetopIndices(SetOpKind::Intersect, A, B, outs[0], nargout, outs,
                         mr, "intersect");
    narrowOut();
}

void setdiff_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("setdiff: requires 2 arguments", 0, 0, "setdiff", "", "numkit:setdiff:nargin");
    auto *mr = ctx.engine->resource();
    const ValueType setNarrow = setopNarrowClass(args[0], args[1]);
    Value A = setopPromote(args[0], mr), B = setopPromote(args[1], mr);
    auto narrowOut = [&]{ if (setNarrow != ValueType::DOUBLE)
                              outs[0] = narrowUniqueClass(outs[0], setNarrow, mr); };
    if (wantsRows(args, 2)) {
        if (nargout >= 2)
            throw Error("setdiff: 'rows' index output (ia) is not yet "
                        "supported in this revision", 0, 0, "setdiff", "",
                        "numkit:setdiff:rowsIdx");
        outs[0] = setOpRows(A, B, SetOpKind::Setdiff, "setdiff", mr);
        narrowOut();
        return;
    }
    outs[0] = setDiff(A, B, mr, wantsStable(args, 2));
    if (nargout >= 2)
        emitSetopIndices(SetOpKind::Setdiff, A, B, outs[0], nargout, outs,
                         mr, "setdiff");
    narrowOut();
}
NK_BIN_SETOP_REG(discretize, discretize)

#undef NK_BIN_SETOP_REG

void primes_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("primes: requires 1 argument",
                     0, 0, "primes", "", "numkit:primes:nargin");
    outs[0] = primes(args[0].toScalar(), ctx.engine->resource());
}

void isprime_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isprime: requires 1 argument",
                     0, 0, "isprime", "", "numkit:isprime:nargin");
    outs[0] = isprime(args[0], ctx.engine->resource());
}

void factor_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("factor: requires 1 argument",
                     0, 0, "factor", "", "numkit:factor:nargin");
    if (!args[0].isScalar())
        throw Error("factor: argument must be a scalar",
                     0, 0, "factor", "", "numkit:factor:notScalar");
    outs[0] = factor(args[0].toScalar(), ctx.engine->resource());
}

void perms_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("perms: requires 1 argument",
                     0, 0, "perms", "", "numkit:perms:nargin");
    outs[0] = perms(args[0], ctx.engine->resource());
}

void factorial_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("factorial: requires 1 argument",
                     0, 0, "factorial", "", "numkit:factorial:nargin");
    outs[0] = factorial(args[0], ctx.engine->resource());
}

void nchoosek_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("nchoosek: requires 2 arguments (n, k)",
                     0, 0, "nchoosek", "", "numkit:nchoosek:nargin");
    auto *mr = ctx.engine->resource();
    // Scalar N -> binomial coefficient; vector V -> all K-combinations (rows).
    if (args[0].isScalar())
        outs[0] = nchoosek(args[0].toScalar(), args[1].toScalar(), mr);
    else
        outs[0] = nchoosekCombinations(args[0], args[1].toScalar(), mr);
}

// ── Pack 16 adapters ─────────────────────────────────────────────────

void setxor_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("setxor: requires 2 arguments",
                     0, 0, "setxor", "", "numkit:setxor:nargin");
    auto *mr = ctx.engine->resource();
    if (wantsRows(args, 2)) {
        if (nargout >= 2)
            throw Error("setxor: 'rows' index outputs (ia, ib) are not yet supported",
                         0, 0, "setxor", "", "numkit:setxor:rowsIdx");
        outs[0] = setxorRows(args[0], args[1], "setxor", mr);
        return;
    }
    outs[0] = setxor(args[0], args[1], mr);
}

void allunique_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("allunique: requires 1 argument",
                     0, 0, "allunique", "", "numkit:allunique:nargin");
    outs[0] = allunique(args[0], ctx.engine->resource());
}

void numunique_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("numunique: requires 1 argument",
                     0, 0, "numunique", "", "numkit:numunique:nargin");
    outs[0] = numunique(args[0], ctx.engine->resource());
}

void ismembertol_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                     CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ismembertol: requires (A, S, [tol])",
                     0, 0, "ismembertol", "", "numkit:ismembertol:nargin");
    double tol = (args.size() >= 3 && !args[2].isEmpty())
                     ? args[2].toScalar()
                     : 1e-6;
    outs[0] = ismembertol(args[0], args[1], tol, ctx.engine->resource());
}

void uniquetol_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("uniquetol: requires (A, [tol])",
                     0, 0, "uniquetol", "", "numkit:uniquetol:nargin");
    double tol = (args.size() >= 2 && !args[1].isEmpty())
                     ? args[1].toScalar()
                     : 1e-6;
    outs[0] = uniquetol(args[0], tol, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
