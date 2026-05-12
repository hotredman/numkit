// libs/builtin/src/language/arrays/predicates.cpp
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
//   vecnorm(A [, p [, dim]])     vector p-norm along dim
//
// All entries comparisons are exact (== 0). MATLAB documents these as
// exact predicates: even 1e-300 in an off-diagonal entry makes
// isdiag/istril/istriu return false.
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr. No std::vector
// scratch — all transient buffers go through ScratchArena/ScratchVec. These
// fns are O(n²) read-only scans, so almost no scratch is needed anyway.

#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>

namespace numkit::builtin {

namespace {

// Helper: column-major linear indexing.
inline size_t lin(size_t i, size_t j, size_t R) { return i + j * R; }

// Matrix-only sanity: 2D, no 3-D pages.
inline void requireMatrix(const Value &A, const char *who)
{
    if (A.dims().is3D())
        throw Error(std::string(who) + ": input must be 2D",
                    0, 0, who, "", std::string("m:") + who + ":Not2D");
}

} // namespace

// ── isbanded ──────────────────────────────────────────────────────────
// True iff A(i,j) == 0 for all (j - i > upper) and (i - j > lower).
// Non-square inputs are allowed (MATLAB accepts m≠n).
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

Value isbanded(const Value &A, long lower, long upper, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "isbanded");
    if (lower < 0 || upper < 0)
        throw Error("isbanded: bandwidths must be non-negative",
                    0, 0, "isbanded", "", "m:isbanded:NegBand");
    return Value::logicalScalar(isbandedImpl(A, lower, upper), mr);
}

// ── isdiag / istril / istriu ──────────────────────────────────────────
Value isdiag(const Value &A, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "isdiag");
    return Value::logicalScalar(isbandedImpl(A, 0, 0), mr);
}

Value istril(const Value &A, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "istril");
    const long R = static_cast<long>(A.dims().rows());
    // Lower-triangular: upper bandwidth == 0.
    return Value::logicalScalar(isbandedImpl(A, R, 0), mr);
}

Value istriu(const Value &A, std::pmr::memory_resource *mr)
{
    requireMatrix(A, "istriu");
    const long C = static_cast<long>(A.dims().cols());
    // Upper-triangular: lower bandwidth == 0.
    return Value::logicalScalar(isbandedImpl(A, 0, C), mr);
}

// ── issymmetric ───────────────────────────────────────────────────────
// Default: A == A.'  (transpose, no conjugation).
// 'skew' opt: A == -A.'
//
// Note: complex inputs use straight transpose (no conj). [1+1i 2; 2 1-1i]
// is symmetric in MATLAB's sense because its (1,2) and (2,1) entries are
// equal as-is.
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

Value issymmetric(const Value &A, bool skew, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(issymmetricImpl(A, skew), mr);
}

// ── ishermitian ───────────────────────────────────────────────────────
// Default: A == A'  (conjugate transpose).
// 'skew' opt: A == -A'
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
    // Real case: hermitian == symmetric, skew-hermitian == antisymmetric.
    return issymmetricImpl(A, skew);
}

Value ishermitian(const Value &A, bool skew, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(ishermitianImpl(A, skew), mr);
}

// ── bandwidth ─────────────────────────────────────────────────────────
// Returns (lower, upper) bandwidths: max distance from diagonal to a
// non-zero entry, in each direction.
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
                0, 0, "bandwidth", "", "m:bandwidth:BadOpt");
}

// ── vecnorm ───────────────────────────────────────────────────────────
// vecnorm(A [, p [, dim]]) — vector p-norm along dim.
//   defaults: p = 2, dim = first non-singleton dimension.
//   p = Inf  → max(|A|)
//   p = -Inf → min(|A|)
//   else     → (sum |A|^p) ^ (1/p)
//
// Output shape matches A with the reduced dim collapsed to length 1.
Value vecnorm(const Value &A, double p, int dim, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("vecnorm: 3-D arrays not supported",
                    0, 0, "vecnorm", "", "m:vecnorm:3D");
    const size_t R = A.dims().rows();
    const size_t C = A.dims().cols();

    // Default dim: first non-singleton (1-based). Empties default to 1.
    if (dim == 0) {
        if (R == 0 && C == 0) dim = 1;
        else if (R != 1)      dim = 1;
        else                  dim = 2;
    }
    if (dim != 1 && dim != 2)
        throw Error("vecnorm: dim must be 1 or 2",
                    0, 0, "vecnorm", "", "m:vecnorm:BadDim");

    auto p_norm = [&](auto getAbs, size_t n) -> double {
        if (n == 0) return 0.0;
        if (std::isinf(p) && p > 0) {
            double m = 0.0;
            for (size_t k = 0; k < n; ++k) {
                double v = getAbs(k);
                if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
                if (v > m) m = v;
            }
            return m;
        }
        if (std::isinf(p) && p < 0) {
            double m = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < n; ++k) {
                double v = getAbs(k);
                if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
                if (v < m) m = v;
            }
            return m;
        }
        if (p == 2.0) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k) {
                double v = getAbs(k);
                s += v * v;
            }
            return std::sqrt(s);
        }
        double s = 0.0;
        for (size_t k = 0; k < n; ++k) s += std::pow(getAbs(k), p);
        return std::pow(s, 1.0 / p);
    };

    auto getAbs = [&](size_t i, size_t j) -> double {
        if (A.isComplex()) return std::abs(A.complexElem(i, j));
        return std::abs(A.elemAsDouble(lin(i, j, R)));
    };

    // Special case: completely empty input (0×0) produces scalar 0
    // (matches MATLAB convention: vecnorm([]) → 0, not 0×0 empty).
    if (R == 0 && C == 0)
        return Value::scalar(0.0, mr);

    if (dim == 1) {
        // Reduce along rows: output is (1 × C). For 0×N input, produces
        // a row of zeros (empty-norm convention). For M×0 input, output
        // is 1×0 (no columns to fill).
        Value out = Value::matrix(1, C, ValueType::DOUBLE, mr);
        if (C == 0) return out;
        double *od = out.doubleDataMut();
        for (size_t j = 0; j < C; ++j) {
            od[j] = p_norm([&](size_t k){ return getAbs(k, j); }, R);
        }
        return out;
    }
    // dim == 2: reduce along cols, output (R × 1). For 0×N → 0×1; for
    // M×0 → M×1 col of zeros.
    Value out = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    if (R == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < R; ++i) {
        od[i] = p_norm([&](size_t k){ return getAbs(i, k); }, C);
    }
    return out;
}

namespace detail {

// Helper: decode optional 'skew' string opt for is{symmetric,hermitian}.
static bool parseSkewOpt(const Value &v, const char *who)
{
    if (!v.isChar() && !v.isString())
        throw Error(std::string(who) + ": option must be 'skew' or 'nonskew'",
                    0, 0, who, "", std::string("m:") + who + ":BadOpt");
    std::string s = v.toString();
    if (s == "skew")     return true;
    if (s == "nonskew")  return false;
    throw Error(std::string(who) + ": option must be 'skew' or 'nonskew'",
                0, 0, who, "", std::string("m:") + who + ":BadOpt");
}

void issymmetric_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("issymmetric: requires (A)",
                    0, 0, "issymmetric", "", "m:issymmetric:nargin");
    bool skew = (args.size() >= 2) && parseSkewOpt(args[1], "issymmetric");
    outs[0] = issymmetric(args[0], skew, ctx.engine->resource());
}

void ishermitian_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ishermitian: requires (A)",
                    0, 0, "ishermitian", "", "m:ishermitian:nargin");
    bool skew = (args.size() >= 2) && parseSkewOpt(args[1], "ishermitian");
    outs[0] = ishermitian(args[0], skew, ctx.engine->resource());
}

void isbanded_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("isbanded: requires (A, lower, upper)",
                    0, 0, "isbanded", "", "m:isbanded:nargin");
    long lower = static_cast<long>(args[1].toScalar());
    long upper = static_cast<long>(args[2].toScalar());
    outs[0] = isbanded(args[0], lower, upper, ctx.engine->resource());
}

void isdiag_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isdiag: requires (A)", 0, 0, "isdiag", "", "m:isdiag:nargin");
    outs[0] = isdiag(args[0], ctx.engine->resource());
}

void istril_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("istril: requires (A)", 0, 0, "istril", "", "m:istril:nargin");
    outs[0] = istril(args[0], ctx.engine->resource());
}

void istriu_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("istriu: requires (A)", 0, 0, "istriu", "", "m:istriu:nargin");
    outs[0] = istriu(args[0], ctx.engine->resource());
}

void bandwidth_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bandwidth: requires (A) or (A, opt)",
                    0, 0, "bandwidth", "", "m:bandwidth:nargin");
    if (args.size() == 1) {
        // Two-output canonical form. Single-output returns lower bandwidth
        // (matches MATLAB behaviour: x = bandwidth(A) → first output).
        auto [lo, up] = bandwidth(args[0], ctx.engine->resource());
        outs[0] = lo;
        if (nargout >= 2 && outs.size() >= 2) outs[1] = up;
        return;
    }
    // (A, 'lower' | 'upper') form.
    if (!args[1].isChar() && !args[1].isString())
        throw Error("bandwidth: option must be 'lower' or 'upper'",
                    0, 0, "bandwidth", "", "m:bandwidth:BadOpt");
    outs[0] = bandwidthOpt(args[0], args[1].toString(), ctx.engine->resource());
}

void vecnorm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("vecnorm: requires (A [, p [, dim]])",
                    0, 0, "vecnorm", "", "m:vecnorm:nargin");
    double p = 2.0;
    int dim = 0;
    if (args.size() >= 2) p = args[1].toScalar();
    if (args.size() >= 3) dim = static_cast<int>(args[2].toScalar());
    outs[0] = vecnorm(args[0], p, dim, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
