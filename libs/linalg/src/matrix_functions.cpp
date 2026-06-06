// libs/linalg/src/matrix_functions.cpp
//
// Matrix functions — expm, logm_sym, sqrtm_sym + engine adapters.
// Migrated 2026-05-25 from libs/builtin/src/language/arrays/matrix.cpp.

#include <numkit/linalg/matrix_functions.hpp>

#include <numkit/linalg/eig.hpp>                  // eig_symmetric
#include <numkit/builtin/internal/la_solve.hpp>   // detail::la_solve
#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace numkit::linalg {

namespace {

// Multiply two n×n column-major matrices: C = A * B (no aliasing).
void matMul(const double *A, const double *B, double *C, std::size_t n)
{
    std::fill(C, C + n * n, 0.0);
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t k = 0; k < n; ++k) {
            const double bkj = B[k + j * n];
            if (bkj == 0.0) continue;
            for (std::size_t i = 0; i < n; ++i)
                C[i + j * n] += A[i + k * n] * bkj;
        }
}

// 1-norm of an n×n matrix.
double mat1Norm(const double *A, std::size_t n)
{
    double mx = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += std::fabs(A[i + j * n]);
        mx = std::max(mx, s);
    }
    return mx;
}

} // anonymous namespace

Value expm(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("expm: input must be a 2D matrix",
                    0, 0, "expm", "", "numkit:expm:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("expm: matrix must be square",
                    0, 0, "expm", "", "numkit:expm:notSquare");
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Padé(6) scaling-and-squaring.
    ScratchArena scratch(mr);
    ScratchVec<double> A_s(n * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + n * n, A_s.begin());

    const double a_norm = mat1Norm(A_s.data(), n);
    int s = 0;
    if (a_norm > 0.5) {
        s = static_cast<int>(std::ceil(std::log2(a_norm / 0.5)));
        if (s < 0) s = 0;
        const double scale = 1.0 / std::pow(2.0, s);
        for (std::size_t i = 0; i < n * n; ++i) A_s[i] *= scale;
    }

    // Padé(6) coefficients (Higham table 10.4).
    static constexpr double c[7] = {
        1.0, 1.0/2.0, 5.0/44.0, 1.0/66.0, 1.0/792.0, 1.0/15840.0, 1.0/665280.0
    };

    // Powers A^2, A^4, A^6.
    ScratchVec<double> A2(n * n, &scratch);
    ScratchVec<double> A4(n * n, &scratch);
    ScratchVec<double> A6(n * n, &scratch);
    matMul(A_s.data(), A_s.data(), A2.data(), n);
    matMul(A2.data(), A2.data(), A4.data(), n);
    matMul(A2.data(), A4.data(), A6.data(), n);

    ScratchVec<double> P(n * n, &scratch);
    ScratchVec<double> Q(n * n, &scratch);
    std::fill(P.begin(), P.end(), 0.0);
    std::fill(Q.begin(), Q.end(), 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        P[i + i * n] = c[0];
        Q[i + i * n] = c[0];
    }
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[1] * A_s[i];
        Q[i] -= c[1] * A_s[i];
    }
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[2] * A2[i];
        Q[i] += c[2] * A2[i];
    }
    ScratchVec<double> A3(n * n, &scratch);
    matMul(A_s.data(), A2.data(), A3.data(), n);
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[3] * A3[i];
        Q[i] -= c[3] * A3[i];
    }
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[4] * A4[i];
        Q[i] += c[4] * A4[i];
    }
    ScratchVec<double> A5(n * n, &scratch);
    matMul(A_s.data(), A4.data(), A5.data(), n);
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[5] * A5[i];
        Q[i] -= c[5] * A5[i];
    }
    for (std::size_t i = 0; i < n * n; ++i) {
        P[i] += c[6] * A6[i];
        Q[i] += c[6] * A6[i];
    }

    // Solve Q * X = P for X.
    auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (!numkit::builtin::detail::la_solve(Q.data(), n, n, P.data(), n,
                                            out.doubleDataMut(), &scratch))
        throw Error("expm: Padé denominator is singular",
                    0, 0, "expm", "", "numkit:expm:singular");

    if (s > 0) {
        ScratchVec<double> tmp(n * n, &scratch);
        double *X = out.doubleDataMut();
        for (int k = 0; k < s; ++k) {
            matMul(X, X, tmp.data(), n);
            std::copy(tmp.begin(), tmp.end(), X);
        }
    }
    return out;
}

namespace {

// Apply scalar function f to symmetric A's eigenvalues and reconstruct:
//   result = V * diag(f(eig)) * V'
Value applyScalarFnSym(const Value &A, double (*f)(double),
                       const char *fnName, const char *errId,
                       std::pmr::memory_resource *mr)
{
    auto [V, D] = eig_symmetric(A, mr);
    const std::size_t n = static_cast<std::size_t>(D.dims().dim(0));
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    const double *Vdata = V.doubleData();
    const double *Ddata = D.doubleData();

    ScratchArena scratch(mr);
    ScratchVec<double> fD(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        const double e = Ddata[i + i * n];
        const double fe = f(e);
        if (!std::isfinite(fe))
            throw Error(std::string(fnName)
                        + ": eigenvalue out of domain (got "
                        + std::to_string(e) + ")",
                        0, 0, fnName, "", errId);
        fD[i] = fe;
    }

    auto out = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *R = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += Vdata[i + k * n] * fD[k] * Vdata[j + k * n];
            R[i + j * n] = s;
        }
    return out;
}

} // anonymous namespace

Value logm_sym(const Value &A, std::pmr::memory_resource *mr)
{
    return applyScalarFnSym(A, [](double x) { return std::log(x); },
                            "logm", "numkit:logm:negativeEigenvalue", mr);
}

Value sqrtm_sym(const Value &A, std::pmr::memory_resource *mr)
{
    return applyScalarFnSym(A, [](double x) { return std::sqrt(x); },
                            "sqrtm", "numkit:sqrtm:negativeEigenvalue", mr);
}

// ────────────────────────────────────────────────────────────────────────
// expmv — action of matrix exponential on a vector
// (Sidje 1998 simplified; fixed Krylov dimension)
// ────────────────────────────────────────────────────────────────────────

Value expmv(double t, const Value &A, const Value &v, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("expmv: A must be a 2D matrix",
                    0, 0, "expmv", "", "numkit:expmv:notMatrix");
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    if (static_cast<std::size_t>(A.dims().dim(1)) != n)
        throw Error("expmv: A must be square",
                    0, 0, "expmv", "", "numkit:expmv:notSquare");
    if (v.numel() != n)
        throw Error("expmv: length(v) must equal size(A, 1)",
                    0, 0, "expmv", "", "numkit:expmv:badV");
    if (n == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // beta = ||v||₂
    double beta = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double vi = v.elemAsDouble(i);
        beta += vi * vi;
    }
    beta = std::sqrt(beta);
    if (beta == 0.0) {
        auto z = Value::matrix(n, 1, ValueType::DOUBLE, mr);
        std::fill(z.doubleDataMut(), z.doubleDataMut() + n, 0.0);
        return z;
    }

    const std::size_t m = std::min<std::size_t>(30, n);   // Krylov dim

    ScratchArena scratch(mr);
    ScratchVec<double> V(n * (m + 1), 0.0, &scratch);     // Arnoldi basis
    ScratchVec<double> H(m * m, 0.0, &scratch);           // square Hessenberg
    ScratchVec<double> work(n, &scratch);

    // V[:, 0] = v / beta
    for (std::size_t i = 0; i < n; ++i) V[i + 0 * n] = v.elemAsDouble(i) / beta;

    const double *Ad = A.doubleData();
    std::size_t m_eff = m;    // may shrink on lucky breakdown

    for (std::size_t j = 0; j < m; ++j) {
        // work = A · V[:, j]
        for (std::size_t i = 0; i < n; ++i) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k) s += Ad[i + k * n] * V[k + j * n];
            work[i] = s;
        }
        // Modified Gram-Schmidt: orthogonalise against V[:, 0..j].
        for (std::size_t i = 0; i <= j; ++i) {
            double hij = 0.0;
            for (std::size_t k = 0; k < n; ++k) hij += V[k + i * n] * work[k];
            if (i < m && j < m) H[i + j * m] = hij;
            for (std::size_t k = 0; k < n; ++k) work[k] -= hij * V[k + i * n];
        }
        // Sub-diagonal entry
        double hjp1 = 0.0;
        for (std::size_t k = 0; k < n; ++k) hjp1 += work[k] * work[k];
        hjp1 = std::sqrt(hjp1);
        if (hjp1 < 1e-14) {
            // Lucky breakdown — exact eigenspace found.
            m_eff = j + 1;
            break;
        }
        // Store sub-diagonal into H (within square block: only if j+1 < m).
        if (j + 1 < m) H[(j + 1) + j * m] = hjp1;
        // V[:, j+1] = work / hjp1
        for (std::size_t k = 0; k < n; ++k) V[k + (j + 1) * n] = work[k] / hjp1;
    }

    // Trim H to m_eff × m_eff if breakdown shortened the basis.
    Value Hsq = Value::matrix(m_eff, m_eff, ValueType::DOUBLE, mr);
    double *Hsd = Hsq.doubleDataMut();
    for (std::size_t j = 0; j < m_eff; ++j)
        for (std::size_t i = 0; i < m_eff; ++i)
            Hsd[i + j * m_eff] = (i < m && j < m) ? H[i + j * m] : 0.0;

    // Scale by t: H · t.
    for (std::size_t k = 0; k < m_eff * m_eff; ++k) Hsd[k] *= t;

    // F = expm(t · H), pick first column. F is m_eff × m_eff.
    auto F = expm(Hsq, mr);
    const double *Fd = F.doubleData();

    // w = beta · V[:, 0..m_eff-1] · F[:, 0]
    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *wd = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (std::size_t k = 0; k < m_eff; ++k) s += V[i + k * n] * Fd[k + 0 * m_eff];
        wd[i] = beta * s;
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void expm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("expm: requires exactly 1 argument",
                    0, 0, "expm", "", "numkit:expm:nargin");
    outs[0] = expm(args[0], ctx.engine->resource());
}

void logm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("logm: requires exactly 1 argument",
                    0, 0, "logm", "", "numkit:logm:nargin");
    outs[0] = logm_sym(args[0], ctx.engine->resource());
}

void sqrtm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("sqrtm: requires exactly 1 argument",
                    0, 0, "sqrtm", "", "numkit:sqrtm:nargin");
    outs[0] = sqrtm_sym(args[0], ctx.engine->resource());
}

// MATLAB signature: w = expmv(t, A, v) — three positional args, t first.
// Some flavours (e.g. expmv from Higham's package) use (A, v) and an
// optional t; we mirror MATLAB's documented order with t leading.
void expmv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 3)
        throw Error("expmv: requires (t, A, v)",
                    0, 0, "expmv", "", "numkit:expmv:nargin");
    const double t = args[0].toScalar();
    outs[0] = expmv(t, args[1], args[2], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
