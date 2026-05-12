// libs/builtin/src/language/arrays/ldl.cpp
//
// MATLAB ldl: block LDL' factorization. v1 implements the Crout LDL'
// algorithm WITHOUT pivoting -- works for all positive- and negative-
// definite matrices, and for indefinite matrices whose principal
// minors avoid zero (most cases in practice). For matrices that
// strictly need Bunch-Kaufman 2x2 pivoting (very rare for small n),
// throws "needs Bunch-Kaufman pivoting" -- see KNOWN GAP at end.
//
//   L * D * L' = A
//
// Where:
//   L is unit lower triangular (1's on diagonal, zeros above)
//   D is diagonal (in v1; full block-diagonal with 2x2 blocks needs
//     Bunch-Kaufman, deferred)
//
// Forms:
//   L            = ldl(A)             1-out: just unit-lower L
//   [L, D]       = ldl(A)             2-out: factors
//   [L, D, P]    = ldl(A)             3-out: P = identity matrix in v1
//   [L, D, p]    = ldl(A, 'vector')   p = 1-based perm vector (1:n in v1)
//   ldl(A, 'upper')                   returns U (unit upper) such that A = U'*D*U
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.
// Scratch allocated through ScratchArena/ScratchVec.
//
// KNOWN GAP: complex Hermitian, sparse input, [L,D,P,C] 4-output sparse
// scaling form, 'tol' arg, and Bunch-Kaufman pivoting (P != I) all
// deferred to v2.

#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace numkit::builtin {

namespace {

// Run Crout LDL' factorization on a square symmetric matrix.
// On entry: A_in is the column-major n x n input (read-only).
// On exit: L_out is unit lower triangular (n x n, col-major); D_out is
// the diagonal of D as a length-n vector. Throws if a near-zero pivot
// is encountered (would require Bunch-Kaufman pivoting).
void croutLDL(const double *A_in, size_t n,
              double *L_out, double *D_out)
{
    // Tolerance for pivot smallness: scale by ||A||_F * eps.
    double anorm = 0.0;
    for (size_t k = 0; k < n * n; ++k) anorm += A_in[k] * A_in[k];
    anorm = std::sqrt(anorm);
    const double tol = std::max(1e-300,
                                anorm * std::numeric_limits<double>::epsilon()
                                    * static_cast<double>(n));

    std::fill(L_out, L_out + n * n, 0.0);

    for (size_t k = 0; k < n; ++k) {
        // D[k] = A(k,k) - sum_{i<k} L(k,i)^2 * D[i]
        double s = A_in[k + k * n];
        for (size_t i = 0; i < k; ++i)
            s -= L_out[k + i * n] * L_out[k + i * n] * D_out[i];
        D_out[k] = s;

        if (std::abs(D_out[k]) < tol)
            throw Error("ldl: zero pivot encountered (matrix needs "
                        "Bunch-Kaufman pivoting; not supported in v1)",
                        0, 0, "ldl", "", "m:ldl:NeedsBKPivoting");

        // L(k, k) = 1
        L_out[k + k * n] = 1.0;

        // For j > k: L(j, k) = (A(j, k) - sum_{i<k} L(j,i)*L(k,i)*D[i]) / D[k]
        const double inv_dk = 1.0 / D_out[k];
        for (size_t j = k + 1; j < n; ++j) {
            double t = A_in[j + k * n];
            for (size_t i = 0; i < k; ++i)
                t -= L_out[j + i * n] * L_out[k + i * n] * D_out[i];
            L_out[j + k * n] = t * inv_dk;
        }
    }
}

// Symmetrise a matrix in place: A_sym = (A + A')/2. Guards against
// tiny FP asymmetry on otherwise-symmetric input.
void symmetrise(double *A, size_t n)
{
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < j; ++i) {
            double avg = 0.5 * (A[i + j * n] + A[j + i * n]);
            A[i + j * n] = avg;
            A[j + i * n] = avg;
        }
}

} // namespace

// 3-output ldl: returns L (unit triangular), D (diagonal), and P
// (identity permutation in v1). Whether L is lower (default) or upper
// is controlled by `upper_form`. P is returned as a matrix unless
// `p_as_vector` is true.
std::tuple<Value, Value, Value>
ldl(const Value &A, bool upper_form, bool p_as_vector, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("ldl: input must be 2D",
                    0, 0, "ldl", "", "m:ldl:Not2D");
    const size_t n = A.dims().rows();
    if (A.dims().cols() != n)
        throw Error("ldl: matrix must be square",
                    0, 0, "ldl", "", "m:ldl:NotSquare");
    if (A.isComplex())
        throw Error("ldl: complex Hermitian input not supported in v1",
                    0, 0, "ldl", "", "m:ldl:NoComplex");

    if (n == 0) {
        Value L0 = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        Value D0 = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        Value P0 = p_as_vector ? Value::matrix(1, 0, ValueType::DOUBLE, mr)
                                : Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return {L0, D0, P0};
    }

    ScratchArena scratch(mr);
    ScratchVec<double> A_sym(n * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + n * n, A_sym.begin());
    symmetrise(A_sym.data(), n);

    ScratchVec<double> L_buf(n * n, &scratch);
    ScratchVec<double> D_buf(n, &scratch);
    croutLDL(A_sym.data(), n, L_buf.data(), D_buf.data());

    // Pack outputs.
    Value L = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *Ld = L.doubleDataMut();
    if (upper_form) {
        // U = L' (unit upper triangular). A = U' * D * U.
        std::fill(Ld, Ld + n * n, 0.0);
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i <= j; ++i)
                Ld[i + j * n] = L_buf[j + i * n];
    } else {
        std::copy(L_buf.begin(), L_buf.end(), Ld);
    }

    Value D = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *Dd = D.doubleDataMut();
    std::fill(Dd, Dd + n * n, 0.0);
    for (size_t k = 0; k < n; ++k) Dd[k + k * n] = D_buf[k];

    Value P;
    if (p_as_vector) {
        P = Value::matrix(1, n, ValueType::DOUBLE, mr);
        double *pd = P.doubleDataMut();
        for (size_t k = 0; k < n; ++k) pd[k] = static_cast<double>(k + 1);
    } else {
        P = Value::matrix(n, n, ValueType::DOUBLE, mr);
        double *pd = P.doubleDataMut();
        std::fill(pd, pd + n * n, 0.0);
        for (size_t k = 0; k < n; ++k) pd[k + k * n] = 1.0;
    }
    return {L, D, P};
}

namespace detail {

void ldl_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ldl: requires (A [, opt...])",
                    0, 0, "ldl", "", "m:ldl:nargin");

    bool upper = false;
    bool vec_perm = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("ldl: optional args must be 'lower'/'upper'/'matrix'/'vector'",
                        0, 0, "ldl", "", "m:ldl:BadOpt");
        std::string s = args[i].toString();
        if      (s == "lower")  upper = false;
        else if (s == "upper")  upper = true;
        else if (s == "matrix") vec_perm = false;
        else if (s == "vector") vec_perm = true;
        else
            throw Error("ldl: unknown option '" + s + "'",
                        0, 0, "ldl", "", "m:ldl:BadOpt");
    }

    auto [L, D, P] = ldl(args[0], upper, vec_perm, ctx.engine->resource());
    outs[0] = L;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = D;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = P;
}

} // namespace detail

} // namespace numkit::builtin
