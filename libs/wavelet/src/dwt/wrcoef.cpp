// libs/wavelet/src/dwt/wrcoef.cpp
//
// 1-D single-band reconstruction — wrcoef.
//
// MATLAB R2025b semantics (verified end-to-end via probe scripts):
//
//   y = wrcoef(type, c, l, wname[, n])
//
// The (c, l) pair is the output of `wavedec`. Layout (numkit convention,
// matching the MATLAB doc):
//
//   c = [cA_N, cD_N, cD_{N-1}, ..., cD_1]
//   l = [|cA_N|, |cD_N|, ..., |cD_1|, |x|]            (length N+2)
//   N = length(l) - 2  (the number of decomposition levels)
//
//   * type ∈ {'a', 'd'} — reconstruct from approximation or detail.
//   * n is the level to keep:
//       'a': n ∈ [0, N], default = N.
//            n = 0 → full reconstruction (= original signal).
//            n ∈ [1, N] → keep cA_N and cD_{n+1}..cD_N, zero cD_1..cD_n,
//            then run waverec.
//       'd': n ∈ [1, N], default = N.
//            Zero cA_N and every cD_k except k = n; then waverec.
//   * wname is the wavelet name (haar, db1..db4, sym2/sym4, coif1).
//     The two-filter form `wrcoef(type, c, l, Lo_R, Hi_R[, n])` is
//     intentionally NOT supported in this release — pass wname instead.
//
// Output y is a row vector of length |x| (matches numkit's wavedec/
// waverec which always emit row outputs). MATLAB preserves the
// orientation of c into y; numkit's wavedec coerces c into a row
// internally, so wrcoef output is row-shaped here too.

#include <numkit/wavelet/dwt/multilevel.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace numkit::wavelet {

namespace {

std::vector<double> vec_from_value(const Value &v)
{
    std::vector<double> out(v.numel());
    for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    return out;
}

Value row_from_vec(const std::vector<double> &v,
                   std::pmr::memory_resource *mr)
{
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

} // anonymous

Value wrcoef(const std::string &type, const Value &c, const Value &l,
             const std::string &wname, int n,
             std::pmr::memory_resource *mr)
{
    if (type != "a" && type != "d")
        throw Error("wrcoef: type must be 'a' or 'd'",
                    0, 0, "wrcoef", "", "numkit:wrcoef:type");

    const size_t Lcount = l.numel();
    if (Lcount < 3)
        throw Error("wrcoef: l must have at least 3 entries",
                    0, 0, "wrcoef", "", "numkit:wrcoef:l");
    const int N = static_cast<int>(Lcount) - 2;

    if (n < 0)
        n = N;            // sentinel: caller passed -1 to mean "default"
    if (type == "a") {
        if (n < 0 || n > N)
            throw Error("wrcoef: for type='a', level must satisfy 0 <= n <= "
                        + std::to_string(N),
                        0, 0, "wrcoef", "", "numkit:wrcoef:level");
    } else { // 'd'
        if (n < 1 || n > N)
            throw Error("wrcoef: for type='d', level must satisfy 1 <= n <= "
                        + std::to_string(N),
                        0, 0, "wrcoef", "", "numkit:wrcoef:level");
    }

    auto cv = vec_from_value(c);
    auto sliceLen = [&](size_t idx) -> size_t {
        return static_cast<size_t>(l.elemAsDouble(idx));
    };

    // Validate that c agrees with l.
    size_t expectedC = sliceLen(0);
    for (int k = 0; k < N; ++k) expectedC += sliceLen(1 + k);
    if (cv.size() != expectedC)
        throw Error("wrcoef: c length does not match l bookkeeping",
                    0, 0, "wrcoef", "", "numkit:wrcoef:cl");

    // Detail at MATLAB level k (1-based, k=1 finest, k=N coarsest)
    // lives in c at L-index (1 + N - k). Compute the [start, end) byte
    // ranges in c for each detail.
    std::vector<std::pair<size_t, size_t>> dRange(N + 1);   // index 1..N
    {
        size_t off = sliceLen(0);
        for (int idxL = 1; idxL <= N; ++idxL) {
            const size_t dlen = sliceLen(idxL);
            const int level   = N - idxL + 1;     // L-pos idxL ↔ level
            dRange[level] = {off, off + dlen};
            off += dlen;
        }
    }

    if (type == "a") {
        // Zero detail levels 1..n  (do nothing if n==0).
        for (int k = 1; k <= n; ++k) {
            for (size_t i = dRange[k].first; i < dRange[k].second; ++i)
                cv[i] = 0.0;
        }
    } else {
        // Zero cA_N.
        for (size_t i = 0; i < sliceLen(0); ++i) cv[i] = 0.0;
        // Zero every detail except level n.
        for (int k = 1; k <= N; ++k) {
            if (k == n) continue;
            for (size_t i = dRange[k].first; i < dRange[k].second; ++i)
                cv[i] = 0.0;
        }
    }

    Value cMod = row_from_vec(cv, mr);
    return waverec(cMod, l, wname, mr);
}

namespace detail {

void wrcoef_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("wrcoef: requires (type, c, l, wname[, n])",
                    0, 0, "wrcoef", "", "numkit:wrcoef:nargin");

    if (!args[0].isChar() && !args[0].isString())
        throw Error("wrcoef: type must be a character vector ('a' or 'd')",
                    0, 0, "wrcoef", "", "numkit:wrcoef:type");
    std::string type = args[0].toString();
    for (auto &ch : type)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    // MATLAB also accepts (Lo_R, Hi_R) in place of wname — guard
    // against that misuse with a clear message.
    if (!args[3].isChar() && !args[3].isString())
        throw Error("wrcoef: numkit only supports the wname form "
                    "wrcoef(type, c, l, wname[, n]). The (Lo_R, Hi_R) "
                    "two-filter form is not implemented.",
                    0, 0, "wrcoef", "", "numkit:wrcoef:wname");
    const std::string wname = args[3].toString();

    int n = -1;     // sentinel meaning "default"
    if (args.size() >= 5) {
        if (args[4].isEmpty()) {
            // empty → keep default
        } else {
            const double nd = args[4].toScalar();
            if (nd < 0.0 || nd != std::floor(nd))
                throw Error("wrcoef: level n must be a non-negative integer",
                            0, 0, "wrcoef", "", "numkit:wrcoef:level");
            n = static_cast<int>(nd);
        }
    }

    outs[0] = wrcoef(type, args[1], args[2], wname, n,
                     ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
