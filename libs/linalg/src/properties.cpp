// libs/linalg/src/properties.cpp
//
// inv / det / trace / rank / cond / normest / rcond — and engine adapters.
// Migrated from libs/builtin/src/language/arrays/{matrix,linalg_extras}.cpp.
//
// rank_of / cond_2norm / normest call `numkit::builtin::svd_values`
// until SVD migrates here (group 4). The include of builtin's
// matrix.hpp covers that one external dependency.

#include <numkit/linalg/properties.hpp>

#include <numkit/builtin/internal/la_solve.hpp>          // detail::la_solve
#include <numkit/builtin/language/arrays/matrix.hpp>     // svd_values

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
                    0, 0, "inv", "", "m:inv:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("inv: matrix must be square",
                    0, 0, "inv", "", "m:inv:notSquare");
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
                    0, 0, "inv", "", "m:inv:singular");
    return out;
}

Value trace(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("trace: input must be a 2D matrix",
                    0, 0, "trace", "", "m:trace:notMatrix");
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
                    0, 0, "det", "", "m:det:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("det: matrix must be square",
                    0, 0, "det", "", "m:det:notSquare");
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
    auto sv = numkit::builtin::svd_values(A, mr);
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
    auto sv = numkit::builtin::svd_values(A, mr);
    const std::size_t k = sv.numel();
    if (k == 0) return Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
    const double *s = sv.doubleData();
    const double sigma_max = s[0];
    const double sigma_min = s[k - 1];
    if (sigma_min <= 0.0)
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    return Value::scalar(sigma_max / sigma_min, mr);
}

Value normest(const Value &A, std::pmr::memory_resource *mr)
{
    auto sv = numkit::builtin::svd_values(A, mr);
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
                    0, 0, "rcond", "", "m:rcond:Not2D");
    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    if (M != N)
        throw Error("rcond: matrix must be square",
                    0, 0, "rcond", "", "m:rcond:NotSquare");
    if (M == 0)
        return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    if (A.isComplex())
        throw Error("rcond: complex input not supported in v1",
                    0, 0, "rcond", "", "m:rcond:NoComplex");

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

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void inv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("inv: requires exactly 1 argument",
                    0, 0, "inv", "", "m:inv:nargin");
    outs[0] = inv(args[0], ctx.engine->resource());
}

void trace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("trace: requires exactly 1 argument",
                    0, 0, "trace", "", "m:trace:nargin");
    outs[0] = trace(args[0], ctx.engine->resource());
}

void det_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("det: requires exactly 1 argument",
                    0, 0, "det", "", "m:det:nargin");
    outs[0] = det(args[0], ctx.engine->resource());
}

void rank_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("rank: requires (A) or (A, tol)",
                    0, 0, "rank", "", "m:rank:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = rank_of(args[0], tol, ctx.engine->resource());
}

void cond_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("cond: requires exactly 1 argument (2-norm only in this revision)",
                    0, 0, "cond", "", "m:cond:nargin");
    outs[0] = cond_2norm(args[0], ctx.engine->resource());
}

void normest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("normest: requires exactly 1 argument",
                    0, 0, "normest", "", "m:normest:nargin");
    outs[0] = normest(args[0], ctx.engine->resource());
}

void rcond_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rcond: requires (A)",
                    0, 0, "rcond", "", "m:rcond:nargin");
    outs[0] = rcond(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
