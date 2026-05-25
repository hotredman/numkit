// libs/builtin/src/language/arrays/lsq.cpp
//
// MATLAB linalg cycle 4 — least-squares variants:
//
//   lsqminnorm(A, B [, tol])  Minimum-norm solution to A*X = B for
//                             rank-deficient A (uses pinv internally).
//   lsqnonneg(C, d)           Non-negative least squares: minimize
//                             ||C*x - d|| subject to x >= 0.
//                             Lawson-Hanson active-set algorithm.
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr; all
// scratch through ScratchArena/ScratchVec.
//
// KNOWN GAPs:
//   lsqminnorm: 'rankWarn' / 'RegularizationFactor' name-value args
//               not implemented (warning toggle / Tikhonov regularization).
//   lsqnonneg:  'options' input (optimset structure), 'problem' input,
//               and the 6th output 'lambda' (Lagrange multipliers)
//               not implemented. The 5-output form returns x, resnorm,
//               residual, exitflag, output struct.

#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/builtin/language/operators/binary_ops.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

// TEMPORARY: lsqminnorm calls pinv (migrated to libs/linalg in the
// "decomp+pseudo" pass). This entire file will migrate to libs/linalg
// in the "solvers" pass (group 9), at which point this include can
// drop. Tracked: PROGRESS.md, libs/linalg migration plan.
#include <numkit/linalg/pseudo_subspace.hpp>

#include "language/operators/la_solve.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace numkit::builtin {

// ── lsqminnorm ────────────────────────────────────────────────────────
// Minimum-norm least-squares: X = pinv(A, tol) * B.
//
// For full-rank A this collapses to the same answer as A\B; for
// rank-deficient A it returns the unique min-norm solution to the
// least-squares problem.
Value lsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D() || B.dims().is3D())
        throw Error("lsqminnorm: inputs must be 2D",
                    0, 0, "lsqminnorm", "", "m:lsqminnorm:Not2D");
    if (A.isComplex() || B.isComplex())
        throw Error("lsqminnorm: complex input not supported in v1",
                    0, 0, "lsqminnorm", "", "m:lsqminnorm:NoComplex");

    const size_t M = A.dims().rows();
    if (B.dims().rows() != M)
        throw Error("lsqminnorm: A and B must have same number of rows",
                    0, 0, "lsqminnorm", "", "m:lsqminnorm:DimMismatch");

    // pinv now lives in libs/linalg (migrated 2026-05-25).
    Value Ap = numkit::linalg::pinv(A, have_tol ? tol_user : -1.0, mr);
    return mtimes(Ap, B, mr);
}

// ── lsqnonneg ─────────────────────────────────────────────────────────
//
// Lawson-Hanson active-set NNLS (Algorithm NNLS, "Solving Least
// Squares Problems", 1974).
//
//   minimize  ||C*x - d||^2  subject to x >= 0
//
// State:
//   P (passive set) — indices where x > 0
//   Z (active set)  — indices where x == 0
//   x — current solution
//
// Outer loop:
//   w = C' * (d - C*x)
//   if Z empty or max(w(Z)) <= tol: stop
//   Move index t of max w(Z) from Z to P
//
//   Inner loop:
//     Solve C(:, P)' * C(:, P) * s_P = C(:, P)' * d
//     If all s_P > 0: x = (s_P pad with zeros), break inner
//     Else find alpha = min_{j in P, s(j)<=0}  x(j) / (x(j) - s(j))
//          x = x + alpha * (s - x)
//          Move all P-indices where x ~= 0 to Z
//
struct NnlsResult {
    Value x;
    double resnorm;
    Value residual;     // d - C*x
    int exitflag;       // 1 = success
    int iterations;     // outer loop count
    std::string algorithm;
    std::string message;
};

NnlsResult
lsqnonneg_impl(const Value &C, const Value &d, std::pmr::memory_resource *mr)
{
    if (C.dims().is3D() || d.dims().is3D())
        throw Error("lsqnonneg: inputs must be 2D",
                    0, 0, "lsqnonneg", "", "m:lsqnonneg:Not2D");
    if (C.isComplex() || d.isComplex())
        throw Error("lsqnonneg: real-only inputs in v1",
                    0, 0, "lsqnonneg", "", "m:lsqnonneg:NoComplex");
    const size_t M = C.dims().rows();
    const size_t N = C.dims().cols();
    if (d.numel() != M)
        throw Error("lsqnonneg: C and d must have compatible sizes",
                    0, 0, "lsqnonneg", "", "m:lsqnonneg:DimMismatch");

    NnlsResult R;
    R.exitflag = 1;
    R.algorithm = "active-set";
    R.message = "Optimization terminated.";
    R.iterations = 0;

    R.x = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *xd = R.x.doubleDataMut();
    std::fill(xd, xd + N, 0.0);

    R.residual = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *rd = R.residual.doubleDataMut();
    for (size_t i = 0; i < M; ++i) rd[i] = d.elemAsDouble(i);
    R.resnorm = 0.0;
    for (size_t i = 0; i < M; ++i) R.resnorm += rd[i] * rd[i];

    if (N == 0) return R;

    ScratchArena scratch(mr);
    const double *Cd = C.doubleData();

    // Tolerance: 10 * eps * norm(C, inf) * max(M, N)  (matches Lawson-Hanson book)
    double cnorm = 0.0;
    for (size_t k = 0; k < M * N; ++k) cnorm = std::max(cnorm, std::abs(Cd[k]));
    const double tol = 10.0 * std::numeric_limits<double>::epsilon()
                            * std::max(cnorm, 1.0)
                            * static_cast<double>(std::max(M, N));

    // Active set Z (true if index is in Z), passive P = !Z.
    ScratchVec<uint8_t> in_Z(N, &scratch);
    std::fill(in_Z.begin(), in_Z.end(), 1);  // start: all in Z (x = 0)

    // Working buffers reused across iterations.
    ScratchVec<double> w(N, &scratch);          // C' * (d - C*x)
    ScratchVec<double> s(N, &scratch);          // trial solution (full size)
    ScratchVec<size_t> Pidx(0, &scratch);       // current passive indices
    ScratchVec<double> CP_buf(0, &scratch);     // submatrix C(:, P), col-major (M × |P|)
    ScratchVec<double> CtC(0, &scratch);        // C(:, P)' * C(:, P), |P| × |P|
    ScratchVec<double> Ctd(0, &scratch);        // C(:, P)' * d, length |P|
    ScratchVec<double> sP(0, &scratch);         // sub-solution, length |P|

    const int max_outer = 3 * static_cast<int>(N);
    int outer_iter = 0;

    auto compute_w = [&]() {
        // residual r = d - C*x  is already in rd
        for (size_t j = 0; j < N; ++j) {
            double s = 0.0;
            for (size_t i = 0; i < M; ++i) s += Cd[i + j * M] * rd[i];
            w[j] = s;
        }
    };

    auto recompute_residual = [&]() {
        for (size_t i = 0; i < M; ++i) {
            double s = 0.0;
            for (size_t j = 0; j < N; ++j) s += Cd[i + j * M] * xd[j];
            rd[i] = d.elemAsDouble(i) - s;
        }
        R.resnorm = 0.0;
        for (size_t i = 0; i < M; ++i) R.resnorm += rd[i] * rd[i];
    };

    while (outer_iter < max_outer) {
        compute_w();

        // Find max w(Z).
        size_t t = N;  // sentinel: no candidate
        double max_w = tol;  // strict >
        for (size_t j = 0; j < N; ++j) {
            if (in_Z[j] && w[j] > max_w) { max_w = w[j]; t = j; }
        }
        if (t == N) break;  // no improving direction; KKT satisfied
        ++outer_iter;

        // Move t from Z to P.
        in_Z[t] = 0;

        // Inner loop: solve LS on current P, restore feasibility.
        for (int inner = 0; inner < max_outer; ++inner) {
            // Build P index list and submatrix CP.
            Pidx.clear();
            for (size_t j = 0; j < N; ++j) if (!in_Z[j]) Pidx.push_back(j);
            const size_t p = Pidx.size();
            CP_buf.resize(M * p);
            for (size_t k = 0; k < p; ++k) {
                const double *src = Cd + Pidx[k] * M;
                std::copy(src, src + M, CP_buf.data() + k * M);
            }
            // Normal equations: (CP' * CP) * sP = CP' * d
            CtC.assign(p * p, 0.0);
            Ctd.assign(p, 0.0);
            for (size_t a = 0; a < p; ++a) {
                for (size_t b = 0; b < p; ++b) {
                    double sum = 0.0;
                    for (size_t i = 0; i < M; ++i)
                        sum += CP_buf[i + a * M] * CP_buf[i + b * M];
                    CtC[a + b * p] = sum;
                }
                double sum = 0.0;
                for (size_t i = 0; i < M; ++i)
                    sum += CP_buf[i + a * M] * d.elemAsDouble(i);
                Ctd[a] = sum;
            }
            // Solve CtC * sP = Ctd via la_solve (LU on small system).
            sP.assign(p, 0.0);
            if (!detail::la_solve(CtC.data(), p, p, Ctd.data(), 1, sP.data(), &scratch)) {
                // Singular sub-system — bail with whatever feasible x we have.
                R.exitflag = 1;
                break;
            }

            // s = full-size, zero on Z
            std::fill(s.begin(), s.end(), 0.0);
            for (size_t k = 0; k < p; ++k) s[Pidx[k]] = sP[k];

            // If all s(P) > 0: accept and break inner loop.
            bool all_pos = true;
            for (size_t k = 0; k < p; ++k) if (sP[k] <= 0.0) { all_pos = false; break; }
            if (all_pos) {
                std::copy(s.begin(), s.end(), xd);
                break;
            }

            // Otherwise: find alpha = min over j in P with s(j)<=0  of x(j)/(x(j)-s(j))
            double alpha = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < p; ++k) {
                size_t j = Pidx[k];
                if (sP[k] <= 0.0) {
                    double denom = xd[j] - sP[k];
                    if (denom > 0.0) {
                        double a = xd[j] / denom;
                        if (a < alpha) alpha = a;
                    }
                }
            }
            if (!std::isfinite(alpha)) { R.exitflag = 1; break; }
            // x = x + alpha * (s - x)
            for (size_t j = 0; j < N; ++j) xd[j] += alpha * (s[j] - xd[j]);
            // Move all j in P with x(j) <= tol to Z.
            for (size_t k = 0; k < p; ++k) {
                size_t j = Pidx[k];
                if (std::abs(xd[j]) < tol) {
                    xd[j] = 0.0;
                    in_Z[j] = 1;
                }
            }
        }
        recompute_residual();
    }

    R.iterations = outer_iter;
    return R;
}

namespace detail {

void lsqminnorm_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lsqminnorm: requires (A, B [, tol])",
                    0, 0, "lsqminnorm", "", "m:lsqminnorm:nargin");
    bool have_tol = (args.size() >= 3);
    double tol = have_tol ? args[2].toScalar() : 0.0;
    outs[0] = lsqminnorm(args[0], args[1], have_tol, tol, ctx.engine->resource());
}

void lsqnonneg_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lsqnonneg: requires (C, d)",
                    0, 0, "lsqnonneg", "", "m:lsqnonneg:nargin");
    auto R = lsqnonneg_impl(args[0], args[1], ctx.engine->resource());
    outs[0] = R.x;
    if (nargout >= 2 && outs.size() >= 2)
        outs[1] = Value::scalar(R.resnorm, ctx.engine->resource());
    if (nargout >= 3 && outs.size() >= 3)
        outs[2] = R.residual;
    if (nargout >= 4 && outs.size() >= 4)
        outs[3] = Value::scalar(static_cast<double>(R.exitflag),
                                ctx.engine->resource());
    if (nargout >= 5 && outs.size() >= 5) {
        Value out_struct = Value::structure(ctx.engine->resource());
        out_struct.structFields()["iterations"] =
            Value::scalar(static_cast<double>(R.iterations),
                          ctx.engine->resource());
        // MATLAB returns these as char arrays (not double-quoted strings).
        out_struct.structFields()["algorithm"] =
            Value::fromString(R.algorithm, ctx.engine->resource());
        out_struct.structFields()["message"] =
            Value::fromString(R.message, ctx.engine->resource());
        outs[4] = std::move(out_struct);
    }
}

} // namespace detail

} // namespace numkit::builtin
