// libs/linalg/src/properties.cpp
//
// inv / det / trace / rank / cond / normest / rcond — and engine adapters.
// Migrated from libs/builtin/src/language/arrays/{matrix,linalg_extras}.cpp.
//
// rank_of / cond_2norm / normest call the SVD kernel from
// numkit/linalg/decompositions.hpp. inv / rcond use the la_solve
// kernel still living in libs/builtin.

#include <numkit/linalg/properties.hpp>

#include <numkit/builtin/internal/la_solve.hpp>   // detail::la_solve
#include <numkit/linalg/decompositions.hpp>       // svd_values
#include <numkit/linalg/eig.hpp>                  // condeig uses eig_symmetric / eig_general_VD
#include <numkit/linalg/norms.hpp>                // cond_pnorm uses norm_*

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace numkit::linalg {

namespace {

// Solve A_buf (m×n column-major) against B_buf (m×nrhs col-major) and
// write the result (n×nrhs) into outX. Returns false on singular /
// rank-deficient / wide A.
bool laSolveWrap(const double *A_buf, std::size_t m, std::size_t n,
                 const double *B_buf, std::size_t nrhs, double *outX,
                 std::pmr::memory_resource *mr)
{
    return numkit::builtin::detail::la_solve(A_buf, m, n, B_buf, nrhs, outX, mr);
}

// Build an n×n identity into a contiguous column-major buffer.
void fillIdentity(double *buf, std::size_t n)
{
    std::fill(buf, buf + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        buf[i + i * n] = 1.0;
}

// In-place LU with partial pivoting on a column-major n×n matrix.
// Returns false on zero pivot (singular). On return, `sign` holds
// (-1)^(number of row swaps). Used by det() only.
bool luPartialPivotInplace(double *A, std::size_t n, int &sign)
{
    sign = 1;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t pivot = k;
        double pmax = std::fabs(A[k + k * n]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = std::fabs(A[i + k * n]);
            if (v > pmax) { pmax = v; pivot = i; }
        }
        if (pmax == 0.0) return false;
        if (pivot != k) {
            for (std::size_t j = 0; j < n; ++j)
                std::swap(A[k + j * n], A[pivot + j * n]);
            sign = -sign;
        }
        const double inv_pivot = 1.0 / A[k + k * n];
        for (std::size_t i = k + 1; i < n; ++i) {
            const double factor = A[i + k * n] * inv_pivot;
            A[i + k * n] = factor;
            for (std::size_t j = k + 1; j < n; ++j)
                A[i + j * n] -= factor * A[k + j * n];
        }
    }
    return true;
}

// Default tolerance for rank-cutoff: max(m,n) * eps(sigma_max).
double defaultRankTol(std::size_t m, std::size_t n, double sigma_max)
{
    return static_cast<double>(std::max(m, n))
         * sigma_max
         * std::numeric_limits<double>::epsilon();
}

// 1-norm of a column-major M×N matrix: max column sum of |a_ij|.
// Used by rcond.
double matrix_one_norm(const double *A, size_t M, size_t N)
{
    if (M == 0 || N == 0) return 0.0;
    double maxv = 0.0;
    for (size_t j = 0; j < N; ++j) {
        double s = 0.0;
        for (size_t i = 0; i < M; ++i) s += std::abs(A[i + j * M]);
        if (s > maxv) maxv = s;
    }
    return maxv;
}

} // anonymous namespace

Value inv(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("inv: input must be a 2D matrix",
                    0, 0, "inv", "", "numkit:inv:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("inv: matrix must be square",
                    0, 0, "inv", "", "numkit:inv:notSquare");
    if (m == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(m * n, &scratch);
    ScratchVec<double> I_buf(n * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + m * n, A_buf.begin());
    fillIdentity(I_buf.data(), n);

    auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (!laSolveWrap(A_buf.data(), m, n, I_buf.data(), n, out.doubleDataMut(), &scratch))
        throw Error("inv: matrix is singular to working precision",
                    0, 0, "inv", "", "numkit:inv:singular");
    return out;
}

Value trace(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("trace: input must be a 2D matrix",
                    0, 0, "trace", "", "numkit:trace:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);
    double s = 0.0;
    const double *p = A.doubleData();
    for (std::size_t i = 0; i < k; ++i)
        s += p[i + i * m];
    return Value::scalar(s, mr);
}

Value det(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("det: input must be a 2D matrix",
                    0, 0, "det", "", "numkit:det:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("det: matrix must be square",
                    0, 0, "det", "", "numkit:det:notSquare");
    if (m == 0)
        return Value::scalar(1.0, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(m * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + m * n, A_buf.begin());

    int sign = 1;
    if (!luPartialPivotInplace(A_buf.data(), n, sign))
        return Value::scalar(0.0, mr);

    long double prod = static_cast<long double>(sign);
    for (std::size_t i = 0; i < n; ++i)
        prod *= static_cast<long double>(A_buf[i + i * n]);
    return Value::scalar(static_cast<double>(prod), mr);
}

Value rank_of(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto sv = svd_values(A, mr);
    const std::size_t k = sv.numel();
    const double *s = sv.doubleData();
    if (k == 0) return Value::scalar(0.0, mr);
    const double sigma_max = s[0];
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;
    int r = 0;
    for (std::size_t i = 0; i < k; ++i)
        if (s[i] > cutoff) ++r;
    return Value::scalar(static_cast<double>(r), mr);
}

Value cond_2norm(const Value &A, std::pmr::memory_resource *mr)
{
    auto sv = svd_values(A, mr);
    const std::size_t k = sv.numel();
    if (k == 0) return Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
    const double *s = sv.doubleData();
    const double sigma_max = s[0];
    const double sigma_min = s[k - 1];
    if (sigma_min <= 0.0)
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    return Value::scalar(sigma_max / sigma_min, mr);
}

// cond(A, p) — generic p-norm condition number.
//   cond(A, p) = ||A||_p · ||inv(A)||_p
// p_kind: 1 → 1-norm, 2 → 2-norm, 3 → Inf-norm, 4 → Frobenius.
Value cond_pnorm(const Value &A, int p_kind, std::pmr::memory_resource *mr)
{
    if (p_kind == 2) return cond_2norm(A, mr);

    if (A.dims().ndim() != 2)
        throw Error("cond: input must be a 2D matrix",
                    0, 0, "cond", "", "numkit:cond:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("cond: matrix must be square for non-2-norm forms",
                    0, 0, "cond", "", "numkit:cond:notSquare");
    if (m == 0)
        return Value::scalar(0.0, mr);

    auto norm_for_kind = [&](const Value &X) -> double {
        switch (p_kind) {
        case 1: return norm_value(X, 1.0, mr).toScalar();
        case 3: return norm_inf(X, mr).toScalar();
        case 4: return norm_fro(X, mr).toScalar();
        default:
            throw Error("cond: p must be 1, 2, Inf, or 'fro'",
                        0, 0, "cond", "", "numkit:cond:badP");
        }
    };

    const double an = norm_for_kind(A);
    if (an == 0.0)
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);

    Value Ainv;
    try {
        Ainv = inv(A, mr);
    } catch (...) {
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    }
    const double in = norm_for_kind(Ainv);
    if (!std::isfinite(in))
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    return Value::scalar(an * in, mr);
}

Value normest(const Value &A, std::pmr::memory_resource *mr)
{
    auto sv = svd_values(A, mr);
    if (sv.numel() == 0) return Value::scalar(0.0, mr);
    return Value::scalar(sv.doubleData()[0], mr);
}

// rcond — reciprocal 1-norm condition estimate (cheap path).
//
// KNOWN GAP: MATLAB uses LAPACK's dgecon (1-norm reverse-iteration
// estimator from Higham 1988). Our impl agrees with MATLAB on
// well-conditioned cases but differs slightly on near-singular A
// because the LAPACK estimator approximates ||inv(A)||_1 without
// computing inv(A) itself.
Value rcond(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("rcond: input must be 2D",
                    0, 0, "rcond", "", "numkit:rcond:Not2D");
    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    if (M != N)
        throw Error("rcond: matrix must be square",
                    0, 0, "rcond", "", "numkit:rcond:NotSquare");
    if (M == 0)
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    if (A.isComplex())
        throw Error("rcond: complex input not supported in v1",
                    0, 0, "rcond", "", "numkit:rcond:NoComplex");

    const double anorm = matrix_one_norm(A.doubleData(), M, N);
    if (anorm == 0.0) return Value::scalar(0.0, mr);

    Value Ainv;
    try {
        Ainv = inv(A, mr);
    } catch (...) {
        return Value::scalar(0.0, mr);
    }
    const double inv_norm = matrix_one_norm(Ainv.doubleData(), M, N);
    if (!std::isfinite(inv_norm) || inv_norm == 0.0)
        return Value::scalar(0.0, mr);
    return Value::scalar(1.0 / (anorm * inv_norm), mr);
}

// condest — 1-norm condition estimate. Reciprocal of rcond (cheap path).
//
// KNOWN GAP: MATLAB's condest uses Higham 1988's blocked 1-norm power-
// iteration estimator (LAPACK dlacn1) which approximates norm(inv(A),1)
// in O(n²) per iteration without forming inv(A). Our impl computes
// norm(A,1) * norm(inv(A),1) exactly. Matches MATLAB on well-conditioned
// A; on near-singular A returns the EXACT condition number while MATLAB
// returns an approximation.
Value condest(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("condest: input must be 2D",
                    0, 0, "condest", "", "numkit:condest:Not2D");
    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    if (M != N)
        throw Error("condest: matrix must be square",
                    0, 0, "condest", "", "numkit:condest:NotSquare");
    if (M == 0)
        return Value::scalar(0.0, mr);

    const double anorm = matrix_one_norm(A.doubleData(), M, N);
    if (anorm == 0.0)
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);

    Value Ainv;
    try {
        Ainv = inv(A, mr);
    } catch (...) {
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    }
    const double inv_norm = matrix_one_norm(Ainv.doubleData(), M, N);
    if (!std::isfinite(inv_norm))
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    return Value::scalar(anorm * inv_norm, mr);
}

// condeig — eigenvalue condition numbers.
//
// For each eigenvalue λ_i with right eigenvector x_i (unit norm) and
// left eigenvector y_i (unit norm, satisfying y_i' A = λ_i y_i'), the
// condition number is
//   s_i = 1 / |y_i' x_i|
// which equals 1/|cos θ_i| where θ_i is the angle between x_i and y_i.
//
// Implementation: V = right eigvecs from eig; left eigvecs are columns
// of inv(V)' (LAPACK convention). Compute s_i = ||V(:,i)|| · ||W(:,i)||
// / |W(:,i)' V(:,i)| where W = inv(V)'. For perfectly conditioned A
// (symmetric / normal), V is orthogonal and s_i == 1 for every i.
//
// Symmetric A short-circuits to a vector of 1s — much cheaper than the
// general path and bit-equal to MATLAB.
Value condeig(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("condeig: input must be a 2D matrix",
                    0, 0, "condeig", "", "numkit:condeig:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("condeig: matrix must be square",
                    0, 0, "condeig", "", "numkit:condeig:notSquare");
    if (n == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // Symmetric short-circuit: every eigenvalue is perfectly conditioned.
    // Tolerance follows the threshold used inside eig_symmetric.
    auto isSymmetric = [&]() -> bool {
        const double *p = A.doubleData();
        const double tol = 1e-10;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) {
                const double d = std::fabs(p[i + j * n] - p[j + i * n]);
                const double s = std::max(std::fabs(p[i + j * n]),
                                           std::fabs(p[j + i * n]));
                if (d > tol * (1.0 + s)) return false;
            }
        return true;
    };
    if (isSymmetric()) {
        auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
        double *o = out.doubleDataMut();
        for (std::size_t i = 0; i < n; ++i) o[i] = 1.0;
        return out;
    }

    // General path. eig_general_VD returns real-eigenvalue cases only;
    // complex eigvecs are deferred to Francis QR.
    auto [V, D] = eig_general_VD(A, mr);

    // W = inv(V)' — its columns are the left eigenvectors. Compute via
    // la_solve on V to get inv(V) directly without going through pinv.
    ScratchArena scratch(mr);
    ScratchVec<double> V_buf(n * n, &scratch);
    ScratchVec<double> I_buf(n * n, 0.0, &scratch);
    std::copy(V.doubleData(), V.doubleData() + n * n, V_buf.begin());
    for (std::size_t i = 0; i < n; ++i) I_buf[i + i * n] = 1.0;

    ScratchVec<double> Vinv(n * n, &scratch);
    if (!numkit::builtin::detail::la_solve(V_buf.data(), n, n, I_buf.data(), n,
                                            Vinv.data(), &scratch))
        throw Error("condeig: right eigenvector matrix is singular",
                    0, 0, "condeig", "", "numkit:condeig:singular");

    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *s = out.doubleDataMut();
    const double *vd = V.doubleData();
    for (std::size_t i = 0; i < n; ++i) {
        // Right eigvec V(:, i)
        double v_norm = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            v_norm += vd[k + i * n] * vd[k + i * n];
        v_norm = std::sqrt(v_norm);

        // Left eigvec = i-th column of W = inv(V)' = i-th ROW of inv(V).
        double w_norm = 0.0;
        for (std::size_t k = 0; k < n; ++k) {
            const double w_k = Vinv[i + k * n];   // inv(V)[i, k]
            w_norm += w_k * w_k;
        }
        w_norm = std::sqrt(w_norm);

        // Inner product y_i' x_i where y_i is the i-th row of inv(V).
        // Right eigvec V(:, i) is column. So y_i' x_i = sum_k Vinv[i, k] * V[k, i].
        double dot = 0.0;
        for (std::size_t k = 0; k < n; ++k)
            dot += Vinv[i + k * n] * vd[k + i * n];

        if (std::fabs(dot) < 1e-300) {
            s[i] = std::numeric_limits<double>::infinity();
        } else {
            s[i] = v_norm * w_norm / std::fabs(dot);
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void inv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("inv: requires exactly 1 argument",
                    0, 0, "inv", "", "numkit:inv:nargin");
    outs[0] = inv(args[0], ctx.engine->resource());
}

void trace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("trace: requires exactly 1 argument",
                    0, 0, "trace", "", "numkit:trace:nargin");
    outs[0] = trace(args[0], ctx.engine->resource());
}

void det_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("det: requires exactly 1 argument",
                    0, 0, "det", "", "numkit:det:nargin");
    outs[0] = det(args[0], ctx.engine->resource());
}

void rank_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("rank: requires (A) or (A, tol)",
                    0, 0, "rank", "", "numkit:rank:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = rank_of(args[0], tol, ctx.engine->resource());
}

void cond_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("cond: requires (A) or (A, p)",
                    0, 0, "cond", "", "numkit:cond:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = cond_2norm(args[0], mr);
        return;
    }
    const Value &p = args[1];
    int p_kind = 2;
    if (p.isChar() || p.isString()) {
        const std::string s = p.toString();
        if      (s == "fro" || s == "Fro") p_kind = 4;
        else if (s == "inf" || s == "Inf") p_kind = 3;
        else throw Error("cond: string p must be 'fro' or 'inf'",
                         0, 0, "cond", "", "numkit:cond:badStringP");
    } else {
        const double pv = p.toScalar();
        if      (pv == 1.0) p_kind = 1;
        else if (pv == 2.0) p_kind = 2;
        else if (std::isinf(pv)) p_kind = 3;
        else throw Error("cond: numeric p must be 1, 2, or Inf",
                         0, 0, "cond", "", "numkit:cond:badP");
    }
    outs[0] = cond_pnorm(args[0], p_kind, mr);
}

void normest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("normest: requires exactly 1 argument",
                    0, 0, "normest", "", "numkit:normest:nargin");
    outs[0] = normest(args[0], ctx.engine->resource());
}

void rcond_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rcond: requires (A)",
                    0, 0, "rcond", "", "numkit:rcond:nargin");
    outs[0] = rcond(args[0], ctx.engine->resource());
}

void condest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("condest: requires (A)",
                    0, 0, "condest", "", "numkit:condest:nargin");
    outs[0] = condest(args[0], ctx.engine->resource());
}

// condeig has three calling forms:
//   s            = condeig(A)
//   [V, D, s]    = condeig(A)        — V, D from eig(A), s as above.
// The 2-output `[V, D] = condeig(A)` is documented in MATLAB only as
// the same as `eig(A)`. Dispatch matches.
void condeig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("condeig: requires (A)",
                    0, 0, "condeig", "", "numkit:condeig:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout <= 1) {
        outs[0] = condeig(args[0], mr);
        return;
    }
    // [V, D[, s]] form: hand off to eig for the first two, compute s
    // separately (no shared work yet — refactor candidate when this
    // becomes a hot path).
    const Value &A = args[0];
    // Eig dispatch matches the eig_reg path.
    auto sym = [&]() -> bool {
        if (A.dims().ndim() != 2) return false;
        const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
        if (n != static_cast<std::size_t>(A.dims().dim(1))) return false;
        const double *p = A.doubleData();
        const double tol = 1e-10;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) {
                const double d = std::fabs(p[i + j * n] - p[j + i * n]);
                const double s = std::max(std::fabs(p[i + j * n]),
                                           std::fabs(p[j + i * n]));
                if (d > tol * (1.0 + s)) return false;
            }
        return true;
    }();
    if (sym) {
        auto [V, D] = eig_symmetric(A, mr);
        outs[0] = std::move(V);
        outs[1] = std::move(D);
    } else {
        auto [V, D] = eig_general_VD(A, mr);
        outs[0] = std::move(V);
        outs[1] = std::move(D);
    }
    if (nargout >= 3 && outs.size() >= 3)
        outs[2] = condeig(A, mr);
}

} // namespace detail

} // namespace numkit::linalg
