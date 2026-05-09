// libs/builtin/src/language/arrays/balance.cpp
//
// MATLAB balance: diagonal-similarity scaling for eigenvalue computations.
// Parlett-Reinsch (1969) algorithm; same as EISPACK `balanc`.
//
//   B = balance(A)              1-out: balanced matrix B
//   [T, B] = balance(A)         T diagonal s.t. B = T \ A * T
//   [S, P, B] = balance(A)      S column of scalings, P column of perms
//   balance(A, 'noperm')        skip permutation phase
//
// v1 implements only the diagonal scaling phase (the permutation phase
// in EISPACK's balanc isolates rows/cols already in upper-triangular
// form; rare in practice and skipped by 'noperm' anyway).
//
// Algorithm (radix = 2):
//   Iterate until no improvement:
//     For each row/col i (excluding diagonal):
//       r = sum |A(i, j)| for j != i
//       c = sum |A(j, i)| for j != i
//       Find power-of-radix factor f minimising c/f + r*f
//       If (c/f + r*f) < 0.95 * (c + r):
//         A(i, :) /= f
//         A(:, i) *= f
//         d[i]    *= f
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.
//
// KNOWN GAP: permutation phase (rows/cols with zero off-diagonal blocks)
// is skipped. Result is identical to MATLAB's balance(A, 'noperm') for
// matrices without isolated eigenvalues. With 'noperm' explicitly
// passed, output is fully MATLAB-equivalent.

#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace numkit::builtin {

namespace {

// Parlett-Reinsch scaling. Modifies B in place; fills d[] with the
// cumulative scaling so that final B = diag(d) \ A_in * diag(d).
void balanceScale(double *B, size_t n, double *d)
{
    constexpr double radix = 2.0;
    constexpr double sqrdx = radix * radix;
    constexpr double threshold = 0.95;

    for (size_t i = 0; i < n; ++i) d[i] = 1.0;
    if (n <= 1) return;

    bool done = false;
    while (!done) {
        done = true;
        for (size_t i = 0; i < n; ++i) {
            double r = 0.0;  // row sum excl. diag
            double c = 0.0;  // col sum excl. diag
            for (size_t j = 0; j < n; ++j) {
                if (j == i) continue;
                r += std::abs(B[i + j * n]);
                c += std::abs(B[j + i * n]);
            }
            if (r == 0.0 || c == 0.0) continue;

            double g = r / radix;
            double f = 1.0;
            const double s = c + r;
            while (c < g) {
                f *= radix;
                c *= sqrdx;
            }
            g = r * radix;
            while (c >= g) {
                f /= radix;
                c /= sqrdx;
            }
            if ((c + r) / f < threshold * s) {
                done = false;
                d[i] *= f;
                // Row i: divide by f.
                for (size_t j = 0; j < n; ++j) B[i + j * n] /= f;
                // Col i: multiply by f.
                for (size_t j = 0; j < n; ++j) B[j + i * n] *= f;
            }
        }
    }
}

} // namespace

// Computes balanced matrix B and the scaling vector d. Caller decides
// whether to package them as 1-out (B), 2-out ([diag(d), B]), or 3-out
// ([d, perm, B]) form. (BalanceResult declared in matrix.hpp.)
//
// v1: noperm is always effectively true. The arg is kept for signature
// compat but doesn't change behaviour beyond documentation.
BalanceResult
balance_impl(std::pmr::memory_resource *mr, const Value &A,
             bool /*noperm*/)
{
    if (A.dims().is3D())
        throw Error("balance: input must be 2D",
                    0, 0, "balance", "", "m:balance:Not2D");
    const size_t n = A.dims().rows();
    if (A.dims().cols() != n)
        throw Error("balance: matrix must be square",
                    0, 0, "balance", "", "m:balance:NotSquare");
    if (A.isComplex())
        throw Error("balance: complex input not supported in v1",
                    0, 0, "balance", "", "m:balance:NoComplex");

    BalanceResult R;
    R.B = Value::matrix(n, n, ValueType::DOUBLE, mr);
    R.d_col = Value::matrix(n == 0 ? 0 : n, n == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    R.perm_col = Value::matrix(n == 0 ? 0 : n, n == 0 ? 0 : 1, ValueType::DOUBLE, mr);

    if (n == 0) return R;

    // Copy A into B, then balance in place.
    std::copy(A.doubleData(), A.doubleData() + n * n, R.B.doubleDataMut());

    ScratchArena scratch(mr);
    ScratchVec<double> d(n, &scratch);
    balanceScale(R.B.doubleDataMut(), n, d.data());

    // Pack outputs.
    double *dd = R.d_col.doubleDataMut();
    double *pd = R.perm_col.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        dd[i] = d[i];
        pd[i] = static_cast<double>(i + 1);  // identity perm
    }
    return R;
}

namespace detail {

void balance_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("balance: requires (A [, 'noperm'])",
                    0, 0, "balance", "", "m:balance:nargin");
    bool noperm = false;
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("balance: optional arg must be 'noperm'",
                        0, 0, "balance", "", "m:balance:BadOpt");
        std::string s = args[1].toString();
        if (s == "noperm") noperm = true;
        else
            throw Error("balance: unknown option '" + s + "'",
                        0, 0, "balance", "", "m:balance:BadOpt");
    }

    auto R = balance_impl(ctx.engine->resource(), args[0], noperm);
    const size_t n = R.B.dims().rows();

    if (nargout <= 1) {
        // 1-out: just B.
        outs[0] = std::move(R.B);
        return;
    }

    if (nargout == 2 || (nargout >= 2 && outs.size() == 2)) {
        // 2-out: [T, B] where T = diag(d).
        Value T = Value::matrix(n, n, ValueType::DOUBLE, ctx.engine->resource());
        double *td = T.doubleDataMut();
        std::fill(td, td + n * n, 0.0);
        const double *dd = R.d_col.doubleData();
        for (size_t i = 0; i < n; ++i) td[i + i * n] = dd[i];
        outs[0] = std::move(T);
        outs[1] = std::move(R.B);
        return;
    }

    // 3-out: [S, P, B] where S = column of scalings, P = column of perms.
    outs[0] = std::move(R.d_col);
    outs[1] = std::move(R.perm_col);
    if (outs.size() >= 3) outs[2] = std::move(R.B);
}

} // namespace detail

} // namespace numkit::builtin
