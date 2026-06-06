// libs/linalg/src/eig.cpp
//
// Eigenvalue family — implementations and engine adapters.
// Migrated 2026-05-25 from libs/builtin/src/language/arrays/matrix.cpp.
//
// Includes:
//   - eig_symmetric (classical Jacobi)
//   - eig_values
//   - poly_of_matrix (Souriau-Faddeev-LeVerrier)
//   - eig_general_values (via poly + roots)
//   - eig_general_VD     (via poly + SVD for null-vectors)
//   - hess / hess_H_only (Householder Hessenberg reduction)
//   - schur_sym          (== eig for symmetric A)
//   - sylvester_sym      (simultaneous diagonalisation)

#include <numkit/linalg/eig.hpp>

#include <numkit/linalg/decompositions.hpp>           // svd_decompose
#include <numkit/linalg/properties.hpp>               // inv (for polyeig)
#include <numkit/builtin/math/poly/polynomials.hpp>   // roots
#include <numkit/builtin/language/arrays/matrix.hpp>  // (header path; symbols moved)
#include <numkit/builtin/language/operators/binary_ops.hpp>  // mtimes (eig(A,B))
#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>
#include <tuple>

namespace numkit::linalg {

// ────────────────────────────────────────────────────────────────────────
// Characteristic polynomial + general eig via roots
// ────────────────────────────────────────────────────────────────────────

Value poly_of_matrix(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("poly: input must be a 2D matrix",
                    0, 0, "poly", "", "numkit:poly:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("poly: matrix must be square (use poly(roots) for vector input)",
                    0, 0, "poly", "", "numkit:poly:notSquare");
    if (n == 0) {
        auto out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = 1.0;
        return out;
    }

    // Souriau-Faddeev-LeVerrier: char poly p(λ) = λ^n + c[1]*λ^{n-1} + ... + c[n].
    //   M_0 = 0  ;  c[0] = 1
    //   for k = 1..n:
    //     M_k = A * M_{k-1} + c[k-1] * A
    //     c[k] = -trace(M_k) / k

    ScratchArena scratch(mr);
    ScratchVec<double> M(n * n, 0.0, &scratch);
    ScratchVec<double> Mnext(n * n, &scratch);

    auto out = Value::matrix(1, n + 1, ValueType::DOUBLE, mr);
    double *c = out.doubleDataMut();
    c[0] = 1.0;

    const double *Adata = A.doubleData();

    for (std::size_t k = 1; k <= n; ++k) {
        // Mnext = A * M
        std::fill(Mnext.begin(), Mnext.end(), 0.0);
        for (std::size_t j = 0; j < n; ++j)
            for (std::size_t kk = 0; kk < n; ++kk) {
                const double mkj = M[kk + j * n];
                if (mkj == 0.0) continue;
                for (std::size_t i = 0; i < n; ++i)
                    Mnext[i + j * n] += Adata[i + kk * n] * mkj;
            }
        // Add c[k-1] * A
        const double cprev = c[k - 1];
        for (std::size_t i = 0; i < n * n; ++i)
            Mnext[i] += cprev * Adata[i];

        // c[k] = -trace(Mnext) / k
        double tr = 0.0;
        for (std::size_t i = 0; i < n; ++i) tr += Mnext[i + i * n];
        c[k] = -tr / static_cast<double>(k);

        std::swap(M, Mnext);
    }
    return out;
}

Value eig_general_values(const Value &A, std::pmr::memory_resource *mr)
{
    auto p = poly_of_matrix(A, mr);
    return numkit::builtin::roots(p, mr);
}

std::tuple<Value, Value>
eig_general_VD(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("eig: input must be a 2D matrix",
                    0, 0, "eig", "", "numkit:eig:notMatrix");
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    if (n != static_cast<std::size_t>(A.dims().dim(1)))
        throw Error("eig: matrix must be square",
                    0, 0, "eig", "", "numkit:eig:notSquare");

    auto eig_vals = eig_general_values(A, mr);
    const std::size_t k = eig_vals.numel();
    if (k != n)
        throw Error("eig: char-poly returned wrong number of eigenvalues",
                    0, 0, "eig", "", "numkit:eig:internalError");

    // Verify all real -- complex eigvecs need Francis QR (deferred).
    if (eig_vals.isComplex()) {
        const Complex *ev = eig_vals.complexData();
        for (std::size_t i = 0; i < k; ++i) {
            if (std::fabs(ev[i].imag()) > 1e-9 * (1.0 + std::fabs(ev[i].real())))
                throw Error("eig: [V, D] form for matrices with complex "
                            "eigenvalues requires Francis QR iteration "
                            "(deferred to Phase 2c-3-future). For "
                            "eigenvalues only, use 'e = eig(A)' (single output).",
                            0, 0, "eig", "", "numkit:eig:complexEigvecs");
        }
    }

    ScratchArena scratch(mr);
    ScratchVec<double> evals(n, &scratch);
    if (eig_vals.isComplex()) {
        const Complex *ev = eig_vals.complexData();
        for (std::size_t i = 0; i < n; ++i) evals[i] = ev[i].real();
    } else {
        const double *ev = eig_vals.doubleData();
        for (std::size_t i = 0; i < n; ++i) evals[i] = ev[i];
    }
    std::sort(evals.begin(), evals.end());

    auto Vout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Dout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *V = Vout.doubleDataMut();
    double *D = Dout.doubleDataMut();
    std::fill(V, V + n * n, 0.0);
    std::fill(D, D + n * n, 0.0);

    const double *Adata = A.doubleData();

    for (std::size_t k2 = 0; k2 < n; ++k2) {
        const double lam = evals[k2];
        D[k2 + k2 * n] = lam;
        auto Ali = Value::matrix(n, n, ValueType::DOUBLE, mr);
        double *AL = Ali.doubleDataMut();
        for (std::size_t i = 0; i < n * n; ++i) AL[i] = Adata[i];
        for (std::size_t i = 0; i < n; ++i) AL[i + i * n] -= lam;
        // Right null vector = last column of V from svd(Ali).
        auto [Us, Ss, Vs] = svd_decompose(Ali, mr);
        const std::size_t nv = static_cast<std::size_t>(Vs.dims().dim(0));
        const double *Vsdata = Vs.doubleData();
        for (std::size_t i = 0; i < n; ++i)
            V[i + k2 * n] = Vsdata[i + (nv - 1) * nv];
    }
    return std::make_tuple(std::move(Vout), std::move(Dout));
}

// ────────────────────────────────────────────────────────────────────────
// Symmetric eig (classical Jacobi)
// ────────────────────────────────────────────────────────────────────────

namespace {

// Classical Jacobi for SYMMETRIC A.
void jacobiSymInplace(double *A, std::size_t n, double *V,
                      std::size_t maxSweeps, double tol)
{
    std::fill(V, V + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) V[i + i * n] = 1.0;
    if (n <= 1) return;

    auto offSum = [&]() {
        double s = 0.0;
        for (std::size_t p = 0; p + 1 < n; ++p)
            for (std::size_t q = p + 1; q < n; ++q)
                s += A[p + q * n] * A[p + q * n];
        return s;
    };

    for (std::size_t sweep = 0; sweep < maxSweeps; ++sweep) {
        if (offSum() < tol * tol) break;
        for (std::size_t p = 0; p + 1 < n; ++p) {
            for (std::size_t q = p + 1; q < n; ++q) {
                const double Apq = A[p + q * n];
                if (std::fabs(Apq) < 1e-30) continue;
                const double App = A[p + p * n];
                const double Aqq = A[q + q * n];
                double c, s;
                if (App == Aqq) {
                    c = 0.7071067811865476;
                    s = (Apq >= 0.0 ? 1.0 : -1.0) * c;
                } else {
                    const double tau = (Aqq - App) / (2.0 * Apq);
                    const double t = (tau >= 0.0)
                        ? 1.0 / (tau + std::sqrt(1.0 + tau * tau))
                        : 1.0 / (tau - std::sqrt(1.0 + tau * tau));
                    c = 1.0 / std::sqrt(1.0 + t * t);
                    s = t * c;
                }

                A[p + p * n] = c * c * App - 2.0 * c * s * Apq + s * s * Aqq;
                A[q + q * n] = s * s * App + 2.0 * c * s * Apq + c * c * Aqq;
                A[p + q * n] = 0.0;
                A[q + p * n] = 0.0;
                for (std::size_t r = 0; r < n; ++r) {
                    if (r == p || r == q) continue;
                    auto get = [&](std::size_t a, std::size_t b) -> double & {
                        return (a < b) ? A[a + b * n] : A[b + a * n];
                    };
                    const double Arp = get(r, p);
                    const double Arq = get(r, q);
                    get(r, p) = c * Arp - s * Arq;
                    get(r, q) = s * Arp + c * Arq;
                }
                for (std::size_t r = 0; r < n; ++r) {
                    const double Vrp = V[r + p * n];
                    const double Vrq = V[r + q * n];
                    V[r + p * n] = c * Vrp - s * Vrq;
                    V[r + q * n] = s * Vrp + c * Vrq;
                }
            }
        }
    }

    // Mirror upper triangle into lower for clean output.
    for (std::size_t p = 0; p + 1 < n; ++p)
        for (std::size_t q = p + 1; q < n; ++q)
            A[q + p * n] = A[p + q * n];
}

bool isSymmetricApprox(const Value &A, double tol)
{
    if (A.dims().ndim() != 2) return false;
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n) return false;
    const double *p = A.doubleData();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j) {
            const double d = std::fabs(p[i + j * n] - p[j + i * n]);
            const double s = std::max(std::fabs(p[i + j * n]),
                                       std::fabs(p[j + i * n]));
            if (d > tol * (1.0 + s)) return false;
        }
    return true;
}

} // anonymous namespace

std::tuple<Value, Value>
eig_symmetric(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("eig: input must be a 2D matrix",
                    0, 0, "eig", "", "numkit:eig:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("eig: matrix must be square",
                    0, 0, "eig", "", "numkit:eig:notSquare");
    if (!isSymmetricApprox(A, 1e-10))
        throw Error("eig: only symmetric matrices supported in this revision "
                    "(general eig via Hessenberg + Francis QR is deferred to Phase 2b)",
                    0, 0, "eig", "", "numkit:eig:notSymmetric");
    if (n == 0) {
        return std::make_tuple(
            Value::matrix(0, 0, ValueType::DOUBLE, mr),
            Value::matrix(0, 0, ValueType::DOUBLE, mr));
    }

    ScratchArena scratch(mr);
    ScratchVec<double> A_work(n * n, &scratch);
    ScratchVec<double> V_work(n * n, &scratch);
    std::copy(A.doubleData(), A.doubleData() + n * n, A_work.begin());
    jacobiSymInplace(A_work.data(), n, V_work.data(),
                     /*maxSweeps=*/64, /*tol=*/1e-13);

    // Sort eigenvalues ASCENDING.
    ScratchVec<std::size_t> order(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) {
                  return A_work[a + a * n] < A_work[b + b * n];
              });

    auto Vout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Dout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *V = Vout.doubleDataMut();
    double *D = Dout.doubleDataMut();
    std::fill(D, D + n * n, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t src = order[k];
        D[k + k * n] = A_work[src + src * n];
        for (std::size_t i = 0; i < n; ++i)
            V[i + k * n] = V_work[i + src * n];
    }
    return std::make_tuple(std::move(Vout), std::move(Dout));
}

Value eig_values(const Value &A, std::pmr::memory_resource *mr)
{
    auto [V, D] = eig_symmetric(A, mr);
    const std::size_t n = static_cast<std::size_t>(D.dims().dim(0));
    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    const double *Ddata = D.doubleData();
    double *o = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) o[i] = Ddata[i + i * n];
    return out;
}

// ────────────────────────────────────────────────────────────────────────
// Hessenberg reduction
// ────────────────────────────────────────────────────────────────────────

namespace {

// In-place Hessenberg reduction via Householder reflectors.
void hessReduceInplace(double *A, std::size_t n, double *P,
                       std::pmr::memory_resource *mr)
{
    std::fill(P, P + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) P[i + i * n] = 1.0;
    if (n < 3) return;

    ScratchArena scratch(mr);
    ScratchVec<double> v_storage(n, &scratch);
    double *v = v_storage.data();

    for (std::size_t k = 0; k + 2 < n; ++k) {
        double norm_sq = 0.0;
        for (std::size_t i = k + 1; i < n; ++i)
            norm_sq += A[i + k * n] * A[i + k * n];
        if (norm_sq == 0.0) continue;
        const double xk = A[k + 1 + k * n];
        const double norm = std::sqrt(norm_sq);
        const double alpha = (xk >= 0.0) ? -norm : norm;
        v[k + 1] = xk - alpha;
        for (std::size_t i = k + 2; i < n; ++i) v[i] = A[i + k * n];
        double v_norm_sq = 0.0;
        for (std::size_t i = k + 1; i < n; ++i) v_norm_sq += v[i] * v[i];
        if (v_norm_sq == 0.0) continue;
        const double tau = 2.0 / v_norm_sq;

        for (std::size_t j = k; j < n; ++j) {
            double dot = 0.0;
            for (std::size_t i = k + 1; i < n; ++i)
                dot += v[i] * A[i + j * n];
            const double s = tau * dot;
            for (std::size_t i = k + 1; i < n; ++i)
                A[i + j * n] -= s * v[i];
        }
        for (std::size_t i = 0; i < n; ++i) {
            double dot = 0.0;
            for (std::size_t j = k + 1; j < n; ++j)
                dot += A[i + j * n] * v[j];
            const double s = tau * dot;
            for (std::size_t j = k + 1; j < n; ++j)
                A[i + j * n] -= s * v[j];
        }
        for (std::size_t i = 0; i < n; ++i) {
            double dot = 0.0;
            for (std::size_t j = k + 1; j < n; ++j)
                dot += P[i + j * n] * v[j];
            const double s = tau * dot;
            for (std::size_t j = k + 1; j < n; ++j)
                P[i + j * n] -= s * v[j];
        }
        A[k + 1 + k * n] = alpha;
        for (std::size_t i = k + 2; i < n; ++i)
            A[i + k * n] = 0.0;
    }
}

} // anonymous namespace

std::tuple<Value, Value>
hess(const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("hess: input must be a 2D matrix",
                    0, 0, "hess", "", "numkit:hess:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("hess: matrix must be square",
                    0, 0, "hess", "", "numkit:hess:notSquare");
    auto Hout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto Pout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (n == 0) return std::make_tuple(std::move(Pout), std::move(Hout));
    std::copy(A.doubleData(), A.doubleData() + n * n, Hout.doubleDataMut());
    hessReduceInplace(Hout.doubleDataMut(), n, Pout.doubleDataMut(), mr);
    return std::make_tuple(std::move(Pout), std::move(Hout));
}

Value hess_H_only(const Value &A, std::pmr::memory_resource *mr)
{
    auto [P, H] = hess(A, mr);
    return H;
}

// ────────────────────────────────────────────────────────────────────────
// Schur (symmetric) + Sylvester (symmetric A, B)
// ────────────────────────────────────────────────────────────────────────

std::tuple<Value, Value>
schur_sym(const Value &A, std::pmr::memory_resource *mr)
{
    // For symmetric A, Schur decomposition is the same as eig.
    return eig_symmetric(A, mr);
}

Value sylvester_sym(const Value &A, const Value &B, const Value &C,
                    std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2 || C.dims().ndim() != 2)
        throw Error("sylvester: A, B, C must be 2D matrices",
                    0, 0, "sylvester", "", "numkit:sylvester:notMatrix");
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t m = static_cast<std::size_t>(B.dims().dim(0));
    if (A.dims().dim(0) != A.dims().dim(1))
        throw Error("sylvester: A must be square",
                    0, 0, "sylvester", "", "numkit:sylvester:badA");
    if (B.dims().dim(0) != B.dims().dim(1))
        throw Error("sylvester: B must be square",
                    0, 0, "sylvester", "", "numkit:sylvester:badB");
    if (C.dims().dim(0) != static_cast<int>(n) ||
        C.dims().dim(1) != static_cast<int>(m))
        throw Error("sylvester: C must be n × m where A is n×n, B is m×m",
                    0, 0, "sylvester", "", "numkit:sylvester:badC");

    auto [Va, Da] = eig_symmetric(A, mr);   // throws if non-sym
    auto [Vb, Db] = eig_symmetric(B, mr);   // throws if non-sym

    const double *Vad = Va.doubleData();
    const double *Dad = Da.doubleData();
    const double *Vbd = Vb.doubleData();
    const double *Dbd = Db.doubleData();
    const double *Cd  = C.doubleData();

    ScratchArena scratch(mr);
    ScratchVec<double> Y(n * m, &scratch);
    ScratchVec<double> tmp(n * m, &scratch);
    // tmp = Va' * C
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += Vad[k + i * n] * Cd[k + j * n];
            tmp[i + j * n] = s;
        }
    // Y = tmp * Vb
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < m; ++k)
                s += tmp[i + k * n] * Vbd[k + j * m];
            Y[i + j * n] = s;
        }

    // y_ij /= (d_a_i + d_b_j)
    for (std::size_t i = 0; i < n; ++i) {
        const double dai = Dad[i + i * n];
        for (std::size_t j = 0; j < m; ++j) {
            const double dbj = Dbd[j + j * m];
            const double denom = dai + dbj;
            if (std::fabs(denom) < 1e-300)
                throw Error("sylvester: A and -B share an eigenvalue (no unique solution)",
                            0, 0, "sylvester", "", "numkit:sylvester:singular");
            Y[i + j * n] /= denom;
        }
    }

    auto out = Value::matrix(n, m, ValueType::DOUBLE, mr);
    double *X = out.doubleDataMut();
    // tmp = Va * Y
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k)
                s += Vad[i + k * n] * Y[k + j * n];
            tmp[i + j * n] = s;
        }
    // X = tmp * Vb'
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < m; ++k)
                s += tmp[i + k * n] * Vbd[j + k * m];
            X[i + j * n] = s;
        }
    return out;
}

// ────────────────────────────────────────────────────────────────────────
// polyeig — polynomial eigenvalue problem via companion linearisation
// ────────────────────────────────────────────────────────────────────────

namespace {

// Build the (k·n × k·n) companion matrix L for the polynomial
// p(λ) = A0 + λ·A1 + … + λ^k·Ak. L has the block form
//
//   L = | 0      I        0      …  0      |
//       | 0      0        I      …  0      |
//       | …                                 |
//       | 0      0        0      …  I      |
//       | -B0    -B1      -B2    …  -B_{k-1} |
//
// where Bi = inv(Ak) * Ai. Eigenvalues of L equal the polynomial
// eigenvalues; right eigenvectors have Kronecker structure
// [x; λ x; λ² x; …; λ^{k-1} x], so the first n rows yield x.
Value buildCompanion(Span<const Value> coeffs, std::pmr::memory_resource *mr)
{
    const std::size_t k = coeffs.size() - 1;          // degree
    const std::size_t n = static_cast<std::size_t>(coeffs[0].dims().dim(0));
    const std::size_t N = k * n;

    // B = inv(Ak) * Ai  for i = 0..k-1.
    // Compute via la_solve once per i — cheaper than forming inv(Ak)
    // explicitly when n is moderate. Here we do form inv() for clarity.
    Value Ak_inv = inv(coeffs[k], mr);

    auto L = Value::matrix(N, N, ValueType::DOUBLE, mr);
    double *Ld = L.doubleDataMut();
    std::fill(Ld, Ld + N * N, 0.0);

    // Upper-diagonal blocks: L[i*n .. (i+1)*n - 1, (i+1)*n .. (i+2)*n - 1] = I.
    for (std::size_t i = 0; i + 1 < k; ++i) {
        for (std::size_t r = 0; r < n; ++r) {
            const std::size_t row = i * n + r;
            const std::size_t col = (i + 1) * n + r;
            Ld[row + col * N] = 1.0;
        }
    }

    // Bottom row of blocks: -Ak_inv * Ai for i = 0..k-1.
    // Compute block product directly into L.
    const double *Akid = Ak_inv.doubleData();
    for (std::size_t i = 0; i < k; ++i) {
        const double *Aid = coeffs[i].doubleData();
        for (std::size_t r = 0; r < n; ++r)
            for (std::size_t c = 0; c < n; ++c) {
                double s = 0.0;
                for (std::size_t p = 0; p < n; ++p)
                    s += Akid[r + p * n] * Aid[p + c * n];
                const std::size_t row = (k - 1) * n + r;
                const std::size_t col = i * n + c;
                Ld[row + col * N] = -s;
            }
    }
    return L;
}

void validatePolyeigCoeffs(Span<const Value> coeffs)
{
    if (coeffs.size() < 2)
        throw Error("polyeig: requires at least 2 coefficient matrices (A0, A1, ...)",
                    0, 0, "polyeig", "", "numkit:polyeig:nargin");
    const std::size_t n = static_cast<std::size_t>(coeffs[0].dims().dim(0));
    if (n != static_cast<std::size_t>(coeffs[0].dims().dim(1)))
        throw Error("polyeig: coefficient matrices must be square",
                    0, 0, "polyeig", "", "numkit:polyeig:notSquare");
    for (std::size_t i = 1; i < coeffs.size(); ++i) {
        if (coeffs[i].dims().ndim() != 2
            || static_cast<std::size_t>(coeffs[i].dims().dim(0)) != n
            || static_cast<std::size_t>(coeffs[i].dims().dim(1)) != n)
            throw Error("polyeig: all coefficient matrices must be n × n with matching n",
                        0, 0, "polyeig", "", "numkit:polyeig:shapeMismatch");
    }
}

} // anonymous namespace

Value polyeig_values(Span<const Value> coeffs, std::pmr::memory_resource *mr)
{
    validatePolyeigCoeffs(coeffs);
    Value L = buildCompanion(coeffs, mr);
    // Eigenvalues of L are the polynomial eigenvalues.
    // The companion is generally non-symmetric → use char-poly path
    // (returns complex column when needed).
    return eig_general_values(L, mr);
}

std::tuple<Value, Value>
polyeig_VE(Span<const Value> coeffs, std::pmr::memory_resource *mr)
{
    validatePolyeigCoeffs(coeffs);
    const std::size_t k = coeffs.size() - 1;
    const std::size_t n = static_cast<std::size_t>(coeffs[0].dims().dim(0));
    const std::size_t N = k * n;

    Value L = buildCompanion(coeffs, mr);
    auto [V_big, D_big] = eig_general_VD(L, mr);   // throws on complex eigvals

    // V is N × N; the polynomial eigenvectors are the top n rows of
    // each column of V_big. Output V: n × N.
    auto V = Value::matrix(n, N, ValueType::DOUBLE, mr);
    auto e = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *Vd = V.doubleDataMut();
    double *ed = e.doubleDataMut();
    const double *Vbd = V_big.doubleData();
    const double *Dbd = D_big.doubleData();

    for (std::size_t j = 0; j < N; ++j) {
        ed[j] = Dbd[j + j * N];
        for (std::size_t r = 0; r < n; ++r)
            Vd[r + j * n] = Vbd[r + j * N];
    }
    return std::make_tuple(std::move(V), std::move(e));
}

// ────────────────────────────────────────────────────────────────────────
// ordeig — read eigenvalues off a (quasi-)triangular Schur factor
// in their stored order (no sorting)
// ────────────────────────────────────────────────────────────────────────

Value ordeig(const Value &T, std::pmr::memory_resource *mr)
{
    if (T.dims().ndim() != 2)
        throw Error("ordeig: T must be a 2D matrix",
                    0, 0, "ordeig", "", "numkit:ordeig:notMatrix");
    const std::size_t n = static_cast<std::size_t>(T.dims().dim(0));
    if (n != static_cast<std::size_t>(T.dims().dim(1)))
        throw Error("ordeig: T must be square",
                    0, 0, "ordeig", "", "numkit:ordeig:notSquare");
    if (n == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    const double *Td = T.doubleData();

    // First pass: detect 2×2 blocks (non-zero sub-diagonal entries
    // T(i+1, i)). Eigenvalues from them are complex conjugate pairs.
    // Real diagonal entries pass through.
    bool any_complex = false;
    std::vector<Complex> out_complex;
    out_complex.reserve(n);

    std::size_t i = 0;
    while (i < n) {
        const double sub = (i + 1 < n) ? Td[(i + 1) + i * n] : 0.0;
        if (std::fabs(sub) <= 1e-14 * (1.0 + std::fabs(Td[i + i * n]))) {
            // Real eigenvalue (diagonal entry).
            out_complex.emplace_back(Td[i + i * n], 0.0);
            ++i;
        } else {
            // 2×2 block at (i, i+1):
            //   [[a c]; [d a']] → eigvals = ((a + a') ± √((a − a')² + 4cd)) / 2
            const double a  = Td[i + i * n];
            const double ap = Td[(i + 1) + (i + 1) * n];
            const double c  = Td[i + (i + 1) * n];
            const double d  = sub;
            const double disc_re = (a - ap) * (a - ap) + 4.0 * c * d;
            const double mean = 0.5 * (a + ap);
            if (disc_re >= 0.0) {
                const double half_sqrt = 0.5 * std::sqrt(disc_re);
                out_complex.emplace_back(mean + half_sqrt, 0.0);
                out_complex.emplace_back(mean - half_sqrt, 0.0);
            } else {
                const double half_imag = 0.5 * std::sqrt(-disc_re);
                out_complex.emplace_back(mean,  half_imag);
                out_complex.emplace_back(mean, -half_imag);
                any_complex = true;
            }
            i += 2;
        }
    }

    if (any_complex) {
        auto out = Value::matrix(n, 1, ValueType::COMPLEX, mr);
        Complex *od = out.complexDataMut();
        for (std::size_t k = 0; k < n; ++k) od[k] = out_complex[k];
        return out;
    }
    auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t k = 0; k < n; ++k) od[k] = out_complex[k].real();
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void ordeig_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("ordeig: requires (T)",
                    0, 0, "ordeig", "", "numkit:ordeig:nargin");
    outs[0] = ordeig(args[0], ctx.engine->resource());
}

void polyeig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyeig: requires at least 2 coefficient matrices",
                    0, 0, "polyeig", "", "numkit:polyeig:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [V, e] = polyeig_VE(args, mr);
        outs[0] = std::move(V);
        outs[1] = std::move(e);
    } else {
        outs[0] = polyeig_values(args, mr);
    }
}

namespace {

// Eigenvalues of M as a column vector, choosing the symmetric (Jacobi) or
// general (char-poly + roots) path automatically.
Value eigValuesAuto(const Value &M, std::pmr::memory_resource *mr)
{
    return isSymmetricApprox(M, 1e-10) ? eig_values(M, mr)
                                       : eig_general_values(M, mr);
}

// [V, D] of M, choosing the symmetric or general path automatically.
std::tuple<Value, Value> eigVDAuto(const Value &M, std::pmr::memory_resource *mr)
{
    return isSymmetricApprox(M, 1e-10) ? eig_symmetric(M, mr)
                                       : eig_general_VD(M, mr);
}

} // namespace

// Left eigenvectors W (the [V,D,W]=eig form): W'·A = D·W', i.e. the columns of
// W are the right eigenvectors of Aᵀ. We eig(Mᵀ), reorder its columns to match
// the eigenvalue order of D, and normalize each to unit 2-norm (MATLAB). For
// symmetric M this reduces to W == V. Only real-eigenvalue M is supported (the
// general eig path itself throws on complex eigenvalues).
static Value leftEigenvectors(const Value &M, const Value &D,
                              std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(M.dims().dim(0));
    // Mᵀ.
    Value Mt = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *mt = Mt.doubleDataMut();
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < n; ++i)
            mt[i + j * n] = M.elemAsDouble(j + i * n);   // Mᵀ(i,j) = M(j,i)

    auto [VL, DL] = eigVDAuto(Mt, mr);

    std::vector<double> d(n), dl(n);
    for (std::size_t k = 0; k < n; ++k) d[k]  = D.elemAsDouble(k + k * n);
    for (std::size_t k = 0; k < n; ++k) dl[k] = DL.elemAsDouble(k + k * n);

    const double *vld = VL.doubleData();
    Value W = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *wd = W.doubleDataMut();
    std::vector<bool> used(n, false);
    for (std::size_t k = 0; k < n; ++k) {
        // Match D's k-th eigenvalue to the nearest unused eigenvalue of Mᵀ.
        std::size_t best = n;
        double bestErr = std::numeric_limits<double>::infinity();
        for (std::size_t j = 0; j < n; ++j) {
            if (used[j]) continue;
            const double e = std::fabs(dl[j] - d[k]);
            if (e < bestErr) { bestErr = e; best = j; }
        }
        used[best] = true;
        double nrm = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            nrm += vld[i + best * n] * vld[i + best * n];
        nrm = std::sqrt(nrm);
        for (std::size_t i = 0; i < n; ++i)
            wd[i + k * n] = (nrm > 0.0) ? vld[i + best * n] / nrm : vld[i + best * n];
    }
    return W;
}

void eig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 3)
        throw Error("eig: requires 1 to 3 arguments",
                    0, 0, "eig", "", "numkit:eig:nargin");
    auto *mr = ctx.engine->resource();

    // Parse trailing args: an optional second matrix B (generalized
    // problem A·v = λ·B·v) and/or a string flag 'vector'/'matrix'
    // ('chol'/'qz'/'nobalance' accepted and ignored — we always reduce
    // the generalized problem to the standard one B\A).
    const Value *A = &args[0];
    const Value *B = nullptr;
    bool wantMatrix = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].type() == ValueType::CHAR) {
            std::string s = args[i].toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "matrix") wantMatrix = true;
            else if (s == "vector") { /* default for the 1-output form */ }
            else if (s == "chol" || s == "qz" || s == "nobalance") { /* accept */ }
            else
                throw Error("eig: unknown option '" + s + "'",
                            0, 0, "eig", "", "numkit:eig:badOption");
        } else {
            if (B)
                throw Error("eig: at most one matrix B is allowed",
                            0, 0, "eig", "", "numkit:eig:tooManyMatrices");
            B = &args[i];
        }
    }

    // Reduce a generalized problem (A, B) to the standard problem on
    // M = B\A = inv(B)·A. The eigenvalues of M equal the generalized
    // eigenvalues, and any eigenvector v of M satisfies A·v = B·v·λ.
    Value M = B ? builtin::mtimes(inv(*B, mr), *A, mr) : *A;

    if (nargout >= 2) {
        auto [V, D] = eigVDAuto(M, mr);
        if (nargout >= 3)
            outs[2] = leftEigenvectors(M, D, mr);   // left eigenvectors
        outs[0] = std::move(V);
        outs[1] = std::move(D);
        return;
    }
    if (wantMatrix) {                       // 'matrix' → diagonal D even with 1 output
        auto [V, D] = eigVDAuto(M, mr);
        (void)V;
        outs[0] = std::move(D);
        return;
    }
    outs[0] = eigValuesAuto(M, mr);
}

void hess_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("hess: requires exactly 1 argument",
                    0, 0, "hess", "", "numkit:hess:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [P, H] = hess(args[0], mr);
        outs[0] = std::move(P);
        outs[1] = std::move(H);
    } else {
        outs[0] = hess_H_only(args[0], mr);
    }
}

void schur_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("schur: requires exactly 1 argument",
                    0, 0, "schur", "", "numkit:schur:nargin");
    auto *mr = ctx.engine->resource();
    auto [U, T] = schur_sym(args[0], mr);
    if (nargout >= 2) {
        outs[0] = std::move(U);
        outs[1] = std::move(T);
    } else {
        outs[0] = std::move(T);
    }
}

void sylvester_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 3)
        throw Error("sylvester: requires (A, B, C)",
                    0, 0, "sylvester", "", "numkit:sylvester:nargin");
    outs[0] = sylvester_sym(args[0], args[1], args[2], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
