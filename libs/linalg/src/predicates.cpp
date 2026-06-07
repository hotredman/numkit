// libs/linalg/src/predicates.cpp
//
// MATLAB matrix-structure predicates and bandwidth queries.
//
//   issymmetric(A [, 'skew'])    bool — A == A.'  (transpose, no conj)
//   ishermitian(A [, 'skew'])    bool — A == A'   (conjugate transpose)
//   isbanded(A, lower, upper)    bool — outside-band entries are zero
//   isdiag(A)                    bool — same as isbanded(A, 0, 0)
//   istril(A)                    bool — same as isbanded(A, n-1, 0)
//   istriu(A)                    bool — same as isbanded(A, 0, n-1)
//   bandwidth(A)                 [lower, upper] (or just lower if 1-out)
//   bandwidth(A, 'lower'|'upper')  scalar
//
// All entries comparisons are exact (== 0).
// Migrated 2026-05-25 from libs/builtin/src/language/arrays/predicates.cpp.

#include <numkit/linalg/predicates.hpp>

// Compute-only TU: Value substrate + Error, no engine. The is*/bandwidth
// builtins (CallContext wrappers) live in predicates_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>

namespace numkit::linalg {

namespace {

inline size_t lin(size_t i, size_t j, size_t R) { return i + j * R; }

inline void requireMatrix(const Value &A, const char *who)
{
    if (A.dims().is3D())
        throw Error(std::string(who) + ": input must be 2D",
                    0, 0, who, "", std::string("numkit:") + who + ":Not2D");
}

bool isbandedImpl(const Value &A, long lower, long upper)
{
    const long R = static_cast<long>(A.dims().rows());
    const long C = static_cast<long>(A.dims().cols());
    if (R == 0 || C == 0) return true;

    const size_t Rs = static_cast<size_t>(R);
    if (A.isComplex()) {
        for (long j = 0; j < C; ++j)
            for (long i = 0; i < R; ++i) {
                if (j - i > upper || i - j > lower) {
                    Complex z = A.complexElem(static_cast<size_t>(i), static_cast<size_t>(j));
                    if (z.real() != 0.0 || z.imag() != 0.0) return false;
                }
            }
        return true;
    }
    for (long j = 0; j < C; ++j)
        for (long i = 0; i < R; ++i) {
            if (j - i > upper || i - j > lower) {
                if (A.elemAsDouble(lin(static_cast<size_t>(i),
                                      static_cast<size_t>(j), Rs)) != 0.0)
                    return false;
            }
        }
    return true;
}

bool issymmetricImpl(const Value &A, bool skew)
{
    requireMatrix(A, "issymmetric");
    const size_t R = A.dims().rows();
    const size_t C = A.dims().cols();
    if (R != C) return false;
    if (A.isComplex()) {
        for (size_t j = 0; j < C; ++j)
            for (size_t i = 0; i <= j; ++i) {
                Complex aij = A.complexElem(i, j);
                Complex aji = A.complexElem(j, i);
                Complex want = skew ? -aji : aji;
                if (aij != want) return false;
            }
        return true;
    }
    for (size_t j = 0; j < C; ++j)
        for (size_t i = 0; i <= j; ++i) {
            double aij = A.elemAsDouble(lin(i, j, R));
            double aji = A.elemAsDouble(lin(j, i, R));
            double want = skew ? -aji : aji;
            if (aij != want) return false;
        }
    return true;
}

bool ishermitianImpl(const Value &A, bool skew)
{
    requireMatrix(A, "ishermitian");
    const size_t R = A.dims().rows();
    const size_t C = A.dims().cols();
    if (R != C) return false;
    if (A.isComplex()) {
        for (size_t j = 0; j < C; ++j)
            for (size_t i = 0; i <= j; ++i) {
                Complex aij = A.complexElem(i, j);
                Complex aji = A.complexElem(j, i);
                Complex conj_aji = std::conj(aji);
                Complex want = skew ? -conj_aji : conj_aji;
                if (aij != want) return false;
            }
        return true;
    }
    return issymmetricImpl(A, skew);
}

std::pair<long, long> bandwidthImpl(const Value &A)
{
    const long R = static_cast<long>(A.dims().rows());
    const long C = static_cast<long>(A.dims().cols());
    long lower = 0, upper = 0;
    if (R == 0 || C == 0) return {0, 0};

    const size_t Rs = static_cast<size_t>(R);
    if (A.isComplex()) {
        for (long j = 0; j < C; ++j)
            for (long i = 0; i < R; ++i) {
                Complex z = A.complexElem(static_cast<size_t>(i), static_cast<size_t>(j));
                if (z.real() == 0.0 && z.imag() == 0.0) continue;
                long d = i - j;
                if (d > 0) lower = std::max(lower, d);
                else        upper = std::max(upper, -d);
            }
    } else {
        for (long j = 0; j < C; ++j)
            for (long i = 0; i < R; ++i) {
                if (A.elemAsDouble(lin(static_cast<size_t>(i),
                                       static_cast<size_t>(j), Rs)) == 0.0)
                    continue;
                long d = i - j;
                if (d > 0) lower = std::max(lower, d);
                else        upper = std::max(upper, -d);
            }
    }
    return {lower, upper};
}

} // anonymous namespace

Value isbanded(const Value &A, long lower, long upper, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "isbanded");
    if (lower < 0 || upper < 0)
        throw Error("isbanded: bandwidths must be non-negative",
                    0, 0, "isbanded", "", "numkit:isbanded:NegBand");
    return Value::logicalScalar(isbandedImpl(A, lower, upper), mr);
}

Value isdiag(const Value &A, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "isdiag");
    return Value::logicalScalar(isbandedImpl(A, 0, 0), mr);
}

Value istril(const Value &A, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "istril");
    const long R = static_cast<long>(A.dims().rows());
    return Value::logicalScalar(isbandedImpl(A, R, 0), mr);
}

Value istriu(const Value &A, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "istriu");
    const long C = static_cast<long>(A.dims().cols());
    return Value::logicalScalar(isbandedImpl(A, 0, C), mr);
}

Value issymmetric(const Value &A, bool skew, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(issymmetricImpl(A, skew), mr);
}

Value ishermitian(const Value &A, bool skew, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(ishermitianImpl(A, skew), mr);
}

std::pair<Value, Value>
bandwidth(const Value &A, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "bandwidth");
    auto [lo, up] = bandwidthImpl(A);
    return {Value::scalar(static_cast<double>(lo), mr),
            Value::scalar(static_cast<double>(up), mr)};
}

Value bandwidthOpt(const Value &A, const std::string &which, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "bandwidth");
    auto [lo, up] = bandwidthImpl(A);
    if (which == "lower") return Value::scalar(static_cast<double>(lo), mr);
    if (which == "upper") return Value::scalar(static_cast<double>(up), mr);
    throw Error("bandwidth: option must be 'lower' or 'upper'",
                0, 0, "bandwidth", "", "numkit:bandwidth:BadOpt");
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

} // namespace numkit::linalg
