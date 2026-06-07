// libs/linalg/src/solvers.cpp
//
// linsolve / lsqminnorm / lsqnonneg — and engine adapters.
// Migrated 2026-05-25 from libs/builtin/src/language/arrays/{matrix,lsq}.cpp.

#include <numkit/linalg/solvers.hpp>

#include <numkit/linalg/pseudo_subspace.hpp>      // pinv
#include <numkit/ops/la_solve.hpp>   // numkit::ops::la_solve
#include <numkit/builtin/language/operators/binary_ops.hpp>  // mtimes

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace numkit::linalg {

Value linsolve(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2)
        throw Error("linsolve: A and B must be 2D matrices",
                    0, 0, "linsolve", "", "numkit:linsolve:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t mb = static_cast<std::size_t>(B.dims().dim(0));
    const std::size_t nrhs = static_cast<std::size_t>(B.dims().dim(1));
    if (m != mb)
        throw Error("linsolve: A and B must have the same number of rows",
                    0, 0, "linsolve", "", "numkit:linsolve:badDims");

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(m * n, &scratch);
    ScratchVec<double> B_buf(m * nrhs, &scratch);
    std::copy(A.doubleData(), A.doubleData() + m * n, A_buf.begin());
    std::copy(B.doubleData(), B.doubleData() + m * nrhs, B_buf.begin());

    auto out = Value::matrix(n, nrhs, ValueType::DOUBLE, mr);
    if (!numkit::ops::la_solve(A_buf.data(), m, n, B_buf.data(), nrhs,
                                            out.doubleDataMut(), &scratch))
        throw Error("linsolve: A is singular or rank-deficient",
                    0, 0, "linsolve", "", "numkit:linsolve:singular");
    return out;
}

Value lsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user,
                 std::pmr::memory_resource *mr)
{
    if (A.dims().is3D() || B.dims().is3D())
        throw Error("lsqminnorm: inputs must be 2D",
                    0, 0, "lsqminnorm", "", "numkit:lsqminnorm:Not2D");
    if (A.isComplex() || B.isComplex())
        throw Error("lsqminnorm: complex input not supported in v1",
                    0, 0, "lsqminnorm", "", "numkit:lsqminnorm:NoComplex");

    const size_t M = A.dims().rows();
    if (B.dims().rows() != M)
        throw Error("lsqminnorm: A and B must have same number of rows",
                    0, 0, "lsqminnorm", "", "numkit:lsqminnorm:DimMismatch");

    Value Ap = pinv(A, have_tol ? tol_user : -1.0, mr);
    return numkit::builtin::mtimes(Ap, B, mr);
}

// ── lsqnonneg ─────────────────────────────────────────────────────────
// Lawson-Hanson active-set NNLS.

struct NnlsResult {
    Value x;
    double resnorm;
    Value residual;
    int exitflag;
    int iterations;
    std::string algorithm;
    std::string message;
};

static NnlsResult
lsqnonneg_impl(const Value &C, const Value &d, std::pmr::memory_resource *mr)
{
    if (C.dims().is3D() || d.dims().is3D())
        throw Error("lsqnonneg: inputs must be 2D",
                    0, 0, "lsqnonneg", "", "numkit:lsqnonneg:Not2D");
    if (C.isComplex() || d.isComplex())
        throw Error("lsqnonneg: real-only inputs in v1",
                    0, 0, "lsqnonneg", "", "numkit:lsqnonneg:NoComplex");
    const size_t M = C.dims().rows();
    const size_t N = C.dims().cols();
    if (d.numel() != M)
        throw Error("lsqnonneg: C and d must have compatible sizes",
                    0, 0, "lsqnonneg", "", "numkit:lsqnonneg:DimMismatch");

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

    double cnorm = 0.0;
    for (size_t k = 0; k < M * N; ++k) cnorm = std::max(cnorm, std::abs(Cd[k]));
    const double tol = 10.0 * std::numeric_limits<double>::epsilon()
                            * std::max(cnorm, 1.0)
                            * static_cast<double>(std::max(M, N));

    ScratchVec<uint8_t> in_Z(N, &scratch);
    std::fill(in_Z.begin(), in_Z.end(), 1);

    ScratchVec<double> w(N, &scratch);
    ScratchVec<double> s(N, &scratch);
    ScratchVec<size_t> Pidx(0, &scratch);
    ScratchVec<double> CP_buf(0, &scratch);
    ScratchVec<double> CtC(0, &scratch);
    ScratchVec<double> Ctd(0, &scratch);
    ScratchVec<double> sP(0, &scratch);

    const int max_outer = 3 * static_cast<int>(N);
    int outer_iter = 0;

    auto compute_w = [&]() {
        for (size_t j = 0; j < N; ++j) {
            double sum = 0.0;
            for (size_t i = 0; i < M; ++i) sum += Cd[i + j * M] * rd[i];
            w[j] = sum;
        }
    };

    auto recompute_residual = [&]() {
        for (size_t i = 0; i < M; ++i) {
            double sum = 0.0;
            for (size_t j = 0; j < N; ++j) sum += Cd[i + j * M] * xd[j];
            rd[i] = d.elemAsDouble(i) - sum;
        }
        R.resnorm = 0.0;
        for (size_t i = 0; i < M; ++i) R.resnorm += rd[i] * rd[i];
    };

    while (outer_iter < max_outer) {
        compute_w();

        size_t t = N;
        double max_w = tol;
        for (size_t j = 0; j < N; ++j) {
            if (in_Z[j] && w[j] > max_w) { max_w = w[j]; t = j; }
        }
        if (t == N) break;
        ++outer_iter;

        in_Z[t] = 0;

        for (int inner = 0; inner < max_outer; ++inner) {
            Pidx.clear();
            for (size_t j = 0; j < N; ++j) if (!in_Z[j]) Pidx.push_back(j);
            const size_t p = Pidx.size();
            CP_buf.resize(M * p);
            for (size_t k = 0; k < p; ++k) {
                const double *src = Cd + Pidx[k] * M;
                std::copy(src, src + M, CP_buf.data() + k * M);
            }
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
            sP.assign(p, 0.0);
            if (!numkit::ops::la_solve(CtC.data(), p, p, Ctd.data(), 1,
                                                    sP.data(), &scratch)) {
                R.exitflag = 1;
                break;
            }

            std::fill(s.begin(), s.end(), 0.0);
            for (size_t k = 0; k < p; ++k) s[Pidx[k]] = sP[k];

            bool all_pos = true;
            for (size_t k = 0; k < p; ++k) if (sP[k] <= 0.0) { all_pos = false; break; }
            if (all_pos) {
                std::copy(s.begin(), s.end(), xd);
                break;
            }

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
            for (size_t j = 0; j < N; ++j) xd[j] += alpha * (s[j] - xd[j]);
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

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void linsolve_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("linsolve: requires (A, B[, opts])",
                    0, 0, "linsolve", "", "numkit:linsolve:nargin");
    // 3rd arg (opts struct) accepted for MATLAB-compat but ignored.
    outs[0] = linsolve(args[0], args[1], ctx.engine->resource());
}

void lsqminnorm_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lsqminnorm: requires (A, B [, tol])",
                    0, 0, "lsqminnorm", "", "numkit:lsqminnorm:nargin");
    bool have_tol = (args.size() >= 3);
    double tol = have_tol ? args[2].toScalar() : 0.0;
    outs[0] = lsqminnorm(args[0], args[1], have_tol, tol, ctx.engine->resource());
}

void lsqnonneg_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lsqnonneg: requires (C, d)",
                    0, 0, "lsqnonneg", "", "numkit:lsqnonneg:nargin");
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
        out_struct.structFields()["algorithm"] =
            Value::fromString(R.algorithm, ctx.engine->resource());
        out_struct.structFields()["message"] =
            Value::fromString(R.message, ctx.engine->resource());
        outs[4] = std::move(out_struct);
    }
}

} // namespace detail

} // namespace numkit::linalg
