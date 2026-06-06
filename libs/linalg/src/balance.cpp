// libs/linalg/src/balance.cpp
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
// Migrated 2026-05-25 from libs/builtin/src/language/arrays/balance.cpp.

#include <numkit/linalg/balance.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace numkit::linalg {

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
            double r = 0.0;
            double c = 0.0;
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
                for (size_t j = 0; j < n; ++j) B[i + j * n] /= f;
                for (size_t j = 0; j < n; ++j) B[j + i * n] *= f;
            }
        }
    }
}

} // namespace

BalanceResult
balance_impl(const Value &A, bool /*noperm*/, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("balance: input must be 2D",
                    0, 0, "balance", "", "numkit:balance:Not2D");
    const size_t n = A.dims().rows();
    if (A.dims().cols() != n)
        throw Error("balance: matrix must be square",
                    0, 0, "balance", "", "numkit:balance:NotSquare");
    if (A.isComplex())
        throw Error("balance: complex input not supported in v1",
                    0, 0, "balance", "", "numkit:balance:NoComplex");

    BalanceResult R;
    R.B = Value::matrix(n, n, ValueType::DOUBLE, mr);
    R.d_col = Value::matrix(n == 0 ? 0 : n, n == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    R.perm_col = Value::matrix(n == 0 ? 0 : n, n == 0 ? 0 : 1, ValueType::DOUBLE, mr);

    if (n == 0) return R;

    std::copy(A.doubleData(), A.doubleData() + n * n, R.B.doubleDataMut());

    ScratchArena scratch(mr);
    ScratchVec<double> d(n, &scratch);
    balanceScale(R.B.doubleDataMut(), n, d.data());

    double *dd = R.d_col.doubleDataMut();
    double *pd = R.perm_col.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        dd[i] = d[i];
        pd[i] = static_cast<double>(i + 1);
    }
    return R;
}

namespace detail {

void balance_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("balance: requires (A [, 'noperm'])",
                    0, 0, "balance", "", "numkit:balance:nargin");
    bool noperm = false;
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("balance: optional arg must be 'noperm'",
                        0, 0, "balance", "", "numkit:balance:BadOpt");
        std::string s = args[1].toString();
        if (s == "noperm") noperm = true;
        else
            throw Error("balance: unknown option '" + s + "'",
                        0, 0, "balance", "", "numkit:balance:BadOpt");
    }

    auto R = balance_impl(args[0], noperm, ctx.engine->resource());
    const size_t n = R.B.dims().rows();

    if (nargout <= 1) {
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

    // 3-out: [S, P, B].
    outs[0] = std::move(R.d_col);
    outs[1] = std::move(R.perm_col);
    if (outs.size() >= 3) outs[2] = std::move(R.B);
}

} // namespace detail

} // namespace numkit::linalg
