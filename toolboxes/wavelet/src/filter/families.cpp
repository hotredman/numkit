// toolboxes/wavelet/src/filter/families.cpp
//
// Family-named scaling filters (dbwavf / coifwavf / symwavf) and
// orthogonal filter quadruple (orthfilt).
//
// MATLAB convention (verified vs R2025b):
//   dbwavf('dbN')     = Lo_R / sqrt(2)        (length 2N, sum = 1)
//   coifwavf('coifK') = Lo_R / sqrt(2)        (length 6K)
//   symwavf('symN')   = Lo_R / sqrt(2)        (length 2N)
// where Lo_R is the synthesis lowpass returned by wfilters('xxx').
//
// orthfilt(W) takes a unit-normalised scaling filter W (sum(W) = 1)
// and emits the four filter banks in MATLAB output order
// [Lo_D, Hi_D, Lo_R, Hi_R]:
//
//   Lo_R[k]    = W[k] * sqrt(2)
//   Lo_D[k]    = Lo_R[N-1-k]                       (time reversal)
//   Hi_R[k]    = (-1)^k * Lo_R[N-1-k]              (QMF, alt-sign reversal)
//   Hi_D[k]    = Hi_R[N-1-k]                       (= reverse(Hi_R))
//
// Internally this lib stores Lo_D (MATLAB naming) under the variable
// `Lo_R` of the FilterBank struct (a long-standing legacy of the
// initial wfilters.cpp commit). Code below treats `fb.Lo_R` as
// "MATLAB Lo_D" and reverses it to obtain MATLAB Lo_R, matching the
// rest of MATLAB's documented output.

#include <numkit/wavelet/filter/families.hpp>
#include <numkit/wavelet/filter/wfilters.hpp>

// Compute-only TU: Value substrate + Error, no engine. The dbwavf /
// coifwavf / symwavf / orthfilt builtins (CallContext wrappers) live in
// filter/families_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace numkit::wavelet {

namespace {

const double SQRT2 = 1.41421356237309504880;
const double INV_SQRT2 = 0.70710678118654752440;

Value rowVec(std::pmr::memory_resource *mr, const std::vector<double> &v)
{
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

// dbwavf / coifwavf / symwavf share the same body — emit Lo_R / sqrt(2).
// 2026-05-08: with the wfilters Lo_D/Lo_R label fix, fb.Lo_R is now the
// MATLAB Lo_R directly (was MATLAB Lo_D under numkit's old swapped
// labels). No reversal needed anymore.
Value family_scaling(std::pmr::memory_resource *mr, const std::string &name)
{
    auto fb = wavelet_filters(name);   // throws on unsupported family
    std::vector<double> v = fb.Lo_R;
    for (auto &x : v) x *= INV_SQRT2;
    return rowVec(mr, v);
}

} // anonymous

// ── Public C++ API (see filter/families.hpp) ──────────────────────────

Value dbwavf(const std::string &name, std::pmr::memory_resource *mr)
{
    if (name.rfind("db", 0) != 0 && name != "haar")
        throw Error("dbwavf: name must be 'haar' or 'dbN' (got '" + name + "')",
                    0, 0, "dbwavf", "", "numkit:dbwavf:name");
    return family_scaling(mr, name);
}

Value coifwavf(const std::string &name, std::pmr::memory_resource *mr)
{
    if (name.rfind("coif", 0) != 0)
        throw Error("coifwavf: name must be 'coifK' (got '" + name + "')",
                    0, 0, "coifwavf", "", "numkit:coifwavf:name");
    return family_scaling(mr, name);
}

Value symwavf(const std::string &name, std::pmr::memory_resource *mr)
{
    if (name.rfind("sym", 0) != 0)
        throw Error("symwavf: name must be 'symN' (got '" + name + "')",
                    0, 0, "symwavf", "", "numkit:symwavf:name");
    return family_scaling(mr, name);
}

OrthfiltResult orthfilt(const Value &W, std::pmr::memory_resource *mr)
{
    const size_t N = W.numel();
    if (N == 0)
        throw Error("orthfilt: scaling filter must be non-empty",
                    0, 0, "orthfilt", "", "numkit:orthfilt:empty");

    std::vector<double> Lo_R(N), Lo_D(N), Hi_R(N), Hi_D(N);
    for (size_t k = 0; k < N; ++k)
        Lo_R[k] = W.elemAsDouble(k) * SQRT2;
    for (size_t k = 0; k < N; ++k)
        Lo_D[k] = Lo_R[N - 1 - k];
    for (size_t k = 0; k < N; ++k) {
        const double s = (k % 2 == 0) ? 1.0 : -1.0;
        Hi_R[k] = s * Lo_R[N - 1 - k];
    }
    for (size_t k = 0; k < N; ++k)
        Hi_D[k] = Hi_R[N - 1 - k];

    return { rowVec(mr, Lo_D), rowVec(mr, Hi_D),
             rowVec(mr, Lo_R), rowVec(mr, Hi_R) };
}

} // namespace numkit::wavelet
