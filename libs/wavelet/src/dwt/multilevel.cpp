// libs/wavelet/src/dwt/multilevel.cpp
//
// Multi-level DWT (wavedec) and reconstruction (waverec) wrap the
// single-level dwt / idwt primitives. The (C, L) layout is the
// MATLAB-canonical packing:
//
//   C = [cA_n, cD_n, cD_{n-1}, ..., cD_1]    (concatenated row vector)
//   L = [|cA_n|, |cD_n|, |cD_{n-1}|, ..., |cD_1|, |x|]
//
// (See MATLAB Wavelet Toolbox `wavedec` doc.) The bookkeeping is
// fragile but very small — `appcoef` / `detcoef` just slice C using
// the cumulative offsets implied by L.

#include <numkit/wavelet/dwt/multilevel.hpp>
#include <numkit/wavelet/dwt/dwt.hpp>
#include <numkit/wavelet/filter/wfilters.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace numkit::wavelet {

namespace {

Value rowFromVec(const std::vector<double> &v,
                 std::pmr::memory_resource *mr) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

std::vector<double> vecFromValue(const Value &v) {
    std::vector<double> out(v.numel());
    for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    return out;
}

} // anonymous

std::pair<Value, Value>
wavedec(const Value &x, int n, const std::string &wname,
        std::pmr::memory_resource *mr)
{
    if (n < 1)
        throw Error("wavedec: level must be ≥ 1",
                    0, 0, "wavedec", "", "m:wavedec:level");

    // Run n successive single-level DWTs on the running approximation.
    // Stash each cD in `details[level-1]` (0-indexed: details[0] = cD_1
    // (finest), details[n-1] = cD_n (coarsest)).
    std::vector<std::vector<double>> details(n);
    Value running = Value::matrix(1, x.numel(), ValueType::DOUBLE, mr);
    {
        double *rd = running.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i) rd[i] = x.elemAsDouble(i);
    }

    Value finalApprox;
    for (int k = 0; k < n; ++k) {
        auto [cA, cD] = dwt(running, wname, mr);
        details[k] = vecFromValue(cD);
        running = cA;
        if (k == n - 1) finalApprox = cA;
    }
    auto approx = vecFromValue(finalApprox);

    // L = [|cA_n|, |cD_n|, |cD_{n-1}|, ..., |cD_1|, |x|]
    std::vector<double> L;
    L.reserve(n + 2);
    L.push_back(static_cast<double>(approx.size()));
    for (int k = n - 1; k >= 0; --k)
        L.push_back(static_cast<double>(details[k].size()));
    L.push_back(static_cast<double>(x.numel()));

    // C = [cA_n, cD_n, cD_{n-1}, ..., cD_1]
    size_t total = approx.size();
    for (auto &d : details) total += d.size();
    std::vector<double> C;
    C.reserve(total);
    C.insert(C.end(), approx.begin(), approx.end());
    for (int k = n - 1; k >= 0; --k)
        C.insert(C.end(), details[k].begin(), details[k].end());

    return {rowFromVec(C, mr), rowFromVec(L, mr)};
}

Value waverec(const Value &C, const Value &L, const std::string &wname,
              std::pmr::memory_resource *mr)
{
    const size_t Lcount = L.numel();
    if (Lcount < 3)
        throw Error("waverec: L must have at least 3 entries (1 level)",
                    0, 0, "waverec", "", "m:waverec:size");
    const int n = static_cast<int>(Lcount) - 2; // number of decomposition levels
    auto sliceLen = [&](size_t idx) -> size_t {
        return static_cast<size_t>(L.elemAsDouble(idx));
    };
    auto Cv = vecFromValue(C);

    // Approximation cA_n is the first L[0] entries of C.
    size_t off = 0;
    const size_t aLen = sliceLen(0);
    if (off + aLen > Cv.size())
        throw Error("waverec: C/L mismatch (approx)",
                    0, 0, "waverec", "", "m:waverec:bounds");
    std::vector<double> running(Cv.begin() + off, Cv.begin() + off + aLen);
    off += aLen;

    // Walk through details from coarsest (L[1]) to finest (L[n]).
    for (int k = 0; k < n; ++k) {
        const size_t dLen = sliceLen(1 + k);
        if (off + dLen > Cv.size())
            throw Error("waverec: C/L mismatch (detail)",
                        0, 0, "waverec", "", "m:waverec:bounds");
        std::vector<double> detail(Cv.begin() + off, Cv.begin() + off + dLen);
        off += dLen;

        // Target reconstruction length: L[2+k] (the next coarser
        // approximation length, or the original signal length at the
        // last step).
        const size_t recLen = sliceLen(2 + k);

        // Pack as Value rows for the idwt API.
        Value cA = rowFromVec(running, mr);
        Value cD = rowFromVec(detail, mr);
        Value next = idwt(cA, cD, wname, static_cast<long long>(recLen), mr);
        running = vecFromValue(next);
    }
    return rowFromVec(running, mr);
}

// Internal: appcoef with explicit Lo_R / Hi_R synthesis filters.
static Value appcoef_with_filters(const Value &C, const Value &L,
                                  const std::vector<double> &Lo_R,
                                  const std::vector<double> &Hi_R,
                                  int level,
                                  std::pmr::memory_resource *mr)
{
    const size_t Lcount = L.numel();
    if (Lcount < 3)
        throw Error("appcoef: L too short",
                    0, 0, "appcoef", "", "m:appcoef:size");
    const int nMax = static_cast<int>(Lcount) - 2;
    if (level < 0) level = nMax;          // default = coarsest
    if (level < 0 || level > nMax)
        throw Error("appcoef: level out of range",
                    0, 0, "appcoef", "", "m:appcoef:level");

    auto Cv = vecFromValue(C);
    auto sliceLen = [&](size_t idx) -> size_t {
        return static_cast<size_t>(L.elemAsDouble(idx));
    };

    std::vector<double> running(Cv.begin(), Cv.begin() + sliceLen(0));
    if (level == nMax) return rowFromVec(running, mr);

    size_t off = sliceLen(0);
    for (int k = 0; k < nMax - level; ++k) {
        const size_t dLen = sliceLen(1 + k);
        std::vector<double> detail(Cv.begin() + off, Cv.begin() + off + dLen);
        off += dLen;
        const size_t recLen = sliceLen(2 + k);
        Value cA = rowFromVec(running, mr);
        Value cD = rowFromVec(detail, mr);
        Value next = idwt_with_filters_pub(cA, cD, Lo_R, Hi_R,
                                           static_cast<long long>(recLen), mr);
        running = vecFromValue(next);
    }
    return rowFromVec(running, mr);
}

Value appcoef(const Value &C, const Value &L, const std::string &wname,
              int level,
              std::pmr::memory_resource *mr)
{
    auto fb = wavelet_filters(wname);
    return appcoef_with_filters(C, L, fb.Lo_R, fb.Hi_R, level, mr);
}

Value detcoef(const Value &C, const Value &L, int level,
              std::pmr::memory_resource *mr)
{
    const size_t Lcount = L.numel();
    if (Lcount < 3)
        throw Error("detcoef: L too short",
                    0, 0, "detcoef", "", "m:detcoef:size");
    const int nMax = static_cast<int>(Lcount) - 2;
    if (level < 1 || level > nMax)
        throw Error("detcoef: level out of range",
                    0, 0, "detcoef", "", "m:detcoef:level");

    // C layout: [cA_n, cD_n, cD_{n-1}, ..., cD_1].
    // Detail at MATLAB level `level` (1=finest, nMax=coarsest) lives
    // at C-index that we walk to from the front:
    //   skip cA_n  : L[0]
    //   skip cD_n, cD_{n-1}, ..., cD_{level+1}  : sum L[1..nMax-level]
    //   then dLen = L[nMax - level + 1]
    auto sliceLen = [&](size_t idx) -> size_t {
        return static_cast<size_t>(L.elemAsDouble(idx));
    };
    size_t off = sliceLen(0);
    for (int k = 0; k < nMax - level; ++k) off += sliceLen(1 + k);
    const size_t dLen = sliceLen(static_cast<size_t>(nMax - level + 1));

    auto Cv = vecFromValue(C);
    if (off + dLen > Cv.size())
        throw Error("detcoef: C/L bounds",
                    0, 0, "detcoef", "", "m:detcoef:bounds");
    std::vector<double> out(Cv.begin() + off, Cv.begin() + off + dLen);
    return rowFromVec(out, mr);
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "m:wavelet:type");
    return v.toString();
}

void wavedec_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("wavedec: requires (x, n, wname)",
                    0, 0, "wavedec", "", "m:wavedec:nargin");
    auto *mr = ctx.engine->resource();
    auto [C, L] = wavedec(args[0], static_cast<int>(args[1].toScalar()),
                          argString(args[2]), mr);
    if (outs.size() >= 1) outs[0] = std::move(C);
    if (outs.size() >= 2) outs[1] = std::move(L);
}

void waverec_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("waverec: requires (C, L, wname)",
                    0, 0, "waverec", "", "m:waverec:nargin");
    outs[0] = waverec(args[0], args[1], argString(args[2]),
                      ctx.engine->resource());
}

void appcoef_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("appcoef: requires (C, L, wname[, level]) or "
                    "(C, L, Lo_R, Hi_R[, level])",
                    0, 0, "appcoef", "", "m:appcoef:nargin");
    auto *mr = ctx.engine->resource();

    // Parse trailing 'mode' / Mode= N-V: only 'sym' supported.
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    auto check_mode_at = [&](size_t start) {
        for (size_t i = start; i + 1 < args.size(); i += 2) {
            if (!(args[i].isChar() || args[i].isString())) break;
            const std::string key = lower(args[i].toString());
            if (key == "mode") {
                const std::string m = lower(args[i + 1].toString());
                if (m != "sym" && m != "symh")
                    throw Error("appcoef: only 'mode'='sym' is implemented "
                                "(got '" + m + "')",
                                0, 0, "appcoef", "", "m:appcoef:mode_nyi");
            }
        }
    };

    int level = -1;
    auto readVec = [](const Value &v) {
        const size_t n = v.numel();
        std::vector<double> out(n);
        for (size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
        return out;
    };

    if (args[2].isChar() || args[2].isString()) {
        // (C, L, wname[, level][, 'mode', extmode])
        size_t i = 3;
        if (i < args.size() && !args[i].isEmpty()
            && args[i].numel() == 1
            && !(args[i].isChar() || args[i].isString())) {
            level = (int)args[i].toScalar();
            ++i;
        }
        check_mode_at(i);
        outs[0] = appcoef(args[0], args[1], args[2].toString(), level, mr);
    } else {
        // (C, L, Lo_R, Hi_R[, level][, 'mode', extmode])
        if (args.size() < 4 || (args[3].isChar() || args[3].isString()))
            throw Error("appcoef: custom-filter form requires "
                        "(C, L, Lo_R, Hi_R)",
                        0, 0, "appcoef", "", "m:appcoef:nargin");
        size_t i = 4;
        if (i < args.size() && !args[i].isEmpty()
            && args[i].numel() == 1
            && !(args[i].isChar() || args[i].isString())) {
            level = (int)args[i].toScalar();
            ++i;
        }
        check_mode_at(i);
        outs[0] = appcoef_with_filters(args[0], args[1],
                                       readVec(args[2]), readVec(args[3]),
                                       level, mr);
    }
}

void detcoef_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("detcoef: requires (C, L[, level[, 'cells']])",
                    0, 0, "detcoef", "", "m:detcoef:nargin");
    auto *mr = ctx.engine->resource();
    // Default level = max (deepest decomposition level) when not supplied.
    // MATLAB R2025b: detcoef(c, l) returns the level numel(L) - 2 detail.
    // Bug fix 2026-05-08: was throwing on the 2-arg form.
    if (args.size() == 2) {
        const long long maxLev =
            static_cast<long long>(args[1].numel()) - 2;
        if (maxLev < 1)
            throw Error("detcoef: L is too short to infer a default level",
                        0, 0, "detcoef", "", "m:detcoef:size");
        outs[0] = detcoef(args[0], args[1], static_cast<int>(maxLev), mr);
        return;
    }

    // Detect 'cells' form: levels arg is a vector AND 4th arg is 'cells'.
    const Value &levArg = args[2];
    const bool hasCellsFlag = (args.size() >= 4)
        && (args[3].isChar() || args[3].isString())
        && [&] {
            std::string s = args[3].toString();
            for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s == "cells";
        }();

    if (hasCellsFlag) {
        // 'cells' form: build a cell array, one detail per requested level.
        const size_t k = levArg.numel();
        Value cellOut = Value::cell(1, k);
        for (size_t i = 0; i < k; ++i) {
            const int lev = static_cast<int>(levArg.elemAsDouble(i));
            cellOut.cellAt(i) = detcoef(args[0], args[1], lev, mr);
        }
        outs[0] = std::move(cellOut);
        return;
    }

    // Single-level scalar form (existing behavior).
    if (levArg.numel() != 1)
        throw Error("detcoef: level must be scalar (or use 'cells' form)",
                    0, 0, "detcoef", "", "m:detcoef:level");
    const int level = static_cast<int>(levArg.toScalar());
    outs[0] = detcoef(args[0], args[1], level, mr);
}

} // namespace detail

} // namespace numkit::wavelet
