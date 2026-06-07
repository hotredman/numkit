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

// Compute-only TU: Value substrate + Error, no engine. The wavedec /
// waverec / appcoef / detcoef builtins (CallContext wrappers) live in
// dwt/multilevel_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
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
                    0, 0, "wavedec", "", "numkit:wavedec:level");

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
                    0, 0, "waverec", "", "numkit:waverec:size");
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
                    0, 0, "waverec", "", "numkit:waverec:bounds");
    std::vector<double> running(Cv.begin() + off, Cv.begin() + off + aLen);
    off += aLen;

    // Walk through details from coarsest (L[1]) to finest (L[n]).
    for (int k = 0; k < n; ++k) {
        const size_t dLen = sliceLen(1 + k);
        if (off + dLen > Cv.size())
            throw Error("waverec: C/L mismatch (detail)",
                        0, 0, "waverec", "", "numkit:waverec:bounds");
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
                    0, 0, "appcoef", "", "numkit:appcoef:size");
    const int nMax = static_cast<int>(Lcount) - 2;
    if (level < 0) level = nMax;          // default = coarsest
    if (level < 0 || level > nMax)
        throw Error("appcoef: level out of range",
                    0, 0, "appcoef", "", "numkit:appcoef:level");

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

// Cross-TU access for the custom-filter appcoef form (used by
// multilevel_reg.cpp).
Value appcoef_with_filters_pub(const Value &C, const Value &L,
                               const std::vector<double> &Lo_R,
                               const std::vector<double> &Hi_R,
                               int level,
                               std::pmr::memory_resource *mr)
{
    return appcoef_with_filters(C, L, Lo_R, Hi_R, level, mr);
}

Value detcoef(const Value &C, const Value &L, int level,
              std::pmr::memory_resource *mr)
{
    const size_t Lcount = L.numel();
    if (Lcount < 3)
        throw Error("detcoef: L too short",
                    0, 0, "detcoef", "", "numkit:detcoef:size");
    const int nMax = static_cast<int>(Lcount) - 2;
    if (level < 1 || level > nMax)
        throw Error("detcoef: level out of range",
                    0, 0, "detcoef", "", "numkit:detcoef:level");

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
                    0, 0, "detcoef", "", "numkit:detcoef:bounds");
    std::vector<double> out(Cv.begin() + off, Cv.begin() + off + dLen);
    return rowFromVec(out, mr);
}

} // namespace numkit::wavelet
