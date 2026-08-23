// toolboxes/linalg/src/eig.cpp
//
// Eigenvalue family — implementations and engine adapters.
// Migrated 2026-05-25 from toolboxes/builtin/src/language/arrays/matrix.cpp.
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
#include "linalg_detail.hpp"

#include <numkit/linalg/decompositions.hpp>           // svd_decompose
#include <numkit/linalg/properties.hpp>               // inv (polyeig companion)
#include <numkit/builtin/polyfun.hpp>   // roots

// Compute-only TU: Value substrate + Error, no engine. The eig / hess /
// schur / sylvester / polyeig / ordeig builtins (CallContext wrappers)
// live in eig_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

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
    if (A.dims().ndim() != 2)
        throw Error("eig: input must be a 2D matrix",
                    0, 0, "eig", "", "numkit:eig:notMatrix");
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    if (n != static_cast<std::size_t>(A.dims().dim(1)))
        throw Error("eig: matrix must be square",
                    0, 0, "eig", "", "numkit:eig:notSquare");
    if (n == 0) return Value::matrix(0, 1, A.type(), mr);

    auto [U, T] = schur_general(A, mr);
    auto out = Value::complexMatrix(n, 1, mr);
    Complex *o = out.complexDataMut();

    if (T.isComplex()) {
        const Complex *td = T.complexData();
        for (std::size_t i = 0; i < n; ++i) o[i] = td[i + i * n];
    } else {
        const double *td = T.doubleData();
        std::size_t i = 0;
        while (i < n) {
            if (i + 1 < n && td[(i + 1) + i * n] != 0.0) {
                double a = td[i + i * n];
                double b = td[i + (i + 1) * n];
                double c = td[(i + 1) + i * n];
                double d = td[(i + 1) + (i + 1) * n];
                double tr = a + d;
                double det = a * d - b * c;
                double disc = tr * tr - 4.0 * det;
                double re = 0.5 * tr;
                double im = 0.5 * std::sqrt(std::max(0.0, -disc));
                o[i] = Complex(re, im);
                o[i + 1] = Complex(re, -im);
                i += 2;
            } else {
                o[i] = Complex(td[i + i * n], 0.0);
                i += 1;
            }
        }
    }
    return detail::narrow_if_real(out, mr);
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
    if (n == 0) {
        return std::make_tuple(
            Value::matrix(0, 0, A.type(), mr),
            Value::matrix(0, 0, A.type(), mr));
    }

    if (!A.isComplex()) {
        auto eig_vals = numkit::builtin::roots(poly_of_matrix(A, mr), mr);
        bool has_complex = false;
        if (eig_vals.isComplex()) {
            const Complex *ev = eig_vals.complexData();
            for (std::size_t i = 0; i < eig_vals.numel(); ++i) {
                if (std::fabs(ev[i].imag()) > 1e-9 * (1.0 + std::fabs(ev[i].real()))) {
                    has_complex = true;
                    break;
                }
            }
        }
        if (!has_complex) {
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
                auto [Us, Ss, Vs] = svd_decompose(Ali, mr);
                const std::size_t nv = static_cast<std::size_t>(Vs.dims().dim(0));
                const double *Vsdata = Vs.doubleData();
                for (std::size_t i = 0; i < n; ++i)
                    V[i + k2 * n] = Vsdata[i + (nv - 1) * nv];
            }
            return std::make_tuple(std::move(Vout), std::move(Dout));
        }
    }

    Value A_c = A.isComplex() ? A : Value::complexMatrix(n, n, mr);
    if (!A.isComplex()) {
        const double *ad = A.doubleData();
        Complex *acd = A_c.complexDataMut();
        for (std::size_t i = 0; i < n * n; ++i) acd[i] = Complex(ad[i], 0.0);
    }

    auto [U, T] = schur_general(A_c, mr);

    ScratchArena scratch(mr);
    ScratchVec<Complex> Tc(n * n, &scratch);
    ScratchVec<Complex> Uc(n * n, &scratch);

    if (T.isComplex()) {
        std::copy(T.complexData(), T.complexData() + n * n, Tc.begin());
    } else {
        const double *td = T.doubleData();
        for (size_t i = 0; i < n * n; ++i) Tc[i] = Complex(td[i], 0.0);
    }

    if (U.isComplex()) {
        std::copy(U.complexData(), U.complexData() + n * n, Uc.begin());
    } else {
        const double *ud = U.doubleData();
        for (size_t i = 0; i < n * n; ++i) Uc[i] = Complex(ud[i], 0.0);
    }

    ScratchVec<Complex> Yc(n * n, Complex(0.0, 0.0), &scratch);

    for (std::size_t k = 0; k < n; ++k) {
        Complex lam = Tc[k + k * n];
        Yc[k + k * n] = Complex(1.0, 0.0);
        for (std::intptr_t i = static_cast<std::intptr_t>(k) - 1; i >= 0; --i) {
            Complex sum(0.0, 0.0);
            for (std::size_t j = static_cast<std::size_t>(i) + 1; j <= k; ++j) {
                sum += Tc[static_cast<std::size_t>(i) + j * n] * Yc[j + k * n];
            }
            Complex denom = Tc[static_cast<std::size_t>(i) + static_cast<std::size_t>(i) * n] - lam;
            if (std::abs(denom) < 1e-14) denom = Complex(1e-14, 0.0);
            Yc[static_cast<std::size_t>(i) + k * n] = -sum / denom;
        }
    }

    auto Vout = Value::complexMatrix(n, n, mr);
    auto Dout = Value::complexMatrix(n, n, mr);
    Complex *Vd = Vout.complexDataMut();
    Complex *Dd = Dout.complexDataMut();
    std::fill(Dd, Dd + n * n, Complex(0.0, 0.0));

    for (std::size_t k = 0; k < n; ++k) {
        Dd[k + k * n] = Tc[k + k * n];
        double norm_sq = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            Complex s(0.0, 0.0);
            for (std::size_t j = 0; j <= k; ++j) {
                s += Uc[i + j * n] * Yc[j + k * n];
            }
            Vd[i + k * n] = s;
            norm_sq += detail::abs_sq(s);
        }
        double norm = std::sqrt(norm_sq);
        if (norm > 0.0) {
            for (std::size_t i = 0; i < n; ++i)
                Vd[i + k * n] /= norm;
        }
    }

    return std::make_tuple(detail::narrow_if_real(Vout, mr), detail::narrow_if_real(Dout, mr));
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

} // anonymous namespace

// Approximate-symmetry predicate — declared in eig.hpp; shared with the
// register-side auto-dispatch in eig_reg.cpp.
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
template <typename T>
void hessReduceInplace(T *A, std::size_t n, T *P,
                       std::pmr::memory_resource *mr)
{
    std::fill(P, P + n * n, T(0));
    for (std::size_t i = 0; i < n; ++i) P[i + i * n] = T(1);
    if (n < 3) return;

    ScratchArena scratch(mr);
    ScratchVec<T> v_storage(n, &scratch);
    T *v = v_storage.data();

    for (std::size_t k = 0; k + 2 < n; ++k) {
        double norm_sq = 0.0;
        for (std::size_t i = k + 1; i < n; ++i)
            norm_sq += detail::abs_sq(A[i + k * n]);
        if (norm_sq == 0.0) continue;
        const T xk = A[k + 1 + k * n];
        const double norm = std::sqrt(norm_sq);

        T alpha;
        if constexpr (detail::is_complex_v<T>) {
            const double abs_xk = std::abs(xk);
            const T phase = (abs_xk > 0.0) ? (xk / abs_xk) : T(1.0, 0.0);
            alpha = -phase * norm;
        } else {
            alpha = (xk >= 0.0) ? -norm : norm;
        }

        v[k + 1] = xk - alpha;
        for (std::size_t i = k + 2; i < n; ++i) v[i] = A[i + k * n];
        double v_norm_sq = 0.0;
        for (std::size_t i = k + 1; i < n; ++i) v_norm_sq += detail::abs_sq(v[i]);
        if (v_norm_sq == 0.0) continue;
        const T tau = T(2.0 / v_norm_sq);

        for (std::size_t j = k; j < n; ++j) {
            T dot = T(0);
            for (std::size_t i = k + 1; i < n; ++i) {
                if constexpr (detail::is_complex_v<T>) {
                    dot += std::conj(v[i]) * A[i + j * n];
                } else {
                    dot += v[i] * A[i + j * n];
                }
            }
            const T s = tau * dot;
            for (std::size_t i = k + 1; i < n; ++i)
                A[i + j * n] -= s * v[i];
        }
        for (std::size_t i = 0; i < n; ++i) {
            T dot = T(0);
            for (std::size_t j = k + 1; j < n; ++j)
                dot += A[i + j * n] * v[j];
            const T s = tau * dot;
            for (std::size_t j = k + 1; j < n; ++j) {
                if constexpr (detail::is_complex_v<T>) {
                    A[i + j * n] -= s * std::conj(v[j]);
                } else {
                    A[i + j * n] -= s * v[j];
                }
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            T dot = T(0);
            for (std::size_t j = k + 1; j < n; ++j)
                dot += P[i + j * n] * v[j];
            const T s = tau * dot;
            for (std::size_t j = k + 1; j < n; ++j) {
                if constexpr (detail::is_complex_v<T>) {
                    P[i + j * n] -= s * std::conj(v[j]);
                } else {
                    P[i + j * n] -= s * v[j];
                }
            }
        }
        A[k + 1 + k * n] = alpha;
        for (std::size_t i = k + 2; i < n; ++i)
            A[i + k * n] = T(0);
    }
}

void complexSchurQR(Complex *H, Complex *Z, std::size_t n)
{
    if (n < 2) return;
    auto h = [&](std::size_t i, std::size_t j) -> Complex & { return H[i + j * n]; };
    auto z = [&](std::size_t i, std::size_t j) -> Complex & { return Z[i + j * n]; };

    const double eps = std::numeric_limits<double>::epsilon();
    const int N = static_cast<int>(n);

    int p = N - 1;
    int iter = 0;

    while (p > 0) {
        int l = p;
        while (l > 0) {
            double s = std::abs(h(l - 1, l - 1)) + std::abs(h(l, l));
            if (s == 0.0) s = 1.0;
            if (std::abs(h(l, l - 1)) <= eps * s) {
                h(l, l - 1) = Complex(0.0, 0.0);
                break;
            }
            --l;
        }
        if (l == p) { p -= 1; iter = 0; continue; }
        if (++iter > 300) break;

        // Wilkinson shift from trailing 2x2 block: H[p-1..p, p-1..p]
        Complex a = h(p - 1, p - 1);
        Complex b = h(p - 1, p);
        Complex c_blk = h(p, p - 1);
        Complex d = h(p, p);

        Complex tr = a + d;
        Complex det = a * d - b * c_blk;
        Complex disc = std::sqrt(tr * tr - 4.0 * det);

        Complex mu1 = 0.5 * (tr + disc);
        Complex mu2 = 0.5 * (tr - disc);

        Complex shift = (std::abs(mu1 - d) < std::abs(mu2 - d)) ? mu1 : mu2;

        if (iter % 30 == 0) {
            shift += Complex(std::abs(c_blk), std::abs(h(p - 1, std::max(0, p - 2))));
        }

        for (int k = l; k < p; ++k) {
            Complex f = (k == l) ? (h(k, k) - shift) : h(k, k - 1);
            Complex g = h(k + 1, k);

            double norm = std::sqrt(detail::abs_sq(f) + detail::abs_sq(g));
            if (norm == 0.0) continue;

            double c = std::abs(f) / norm;
            Complex phase = (std::abs(f) > 0.0) ? (f / std::abs(f)) : Complex(1.0, 0.0);
            Complex s = phase * std::conj(g) / norm;

            int col0 = (k > 0) ? (k - 1) : 0;
            for (int j = col0; j < N; ++j) {
                Complex hk = h(k, j);
                Complex hk1 = h(k + 1, j);
                h(k, j)     = c * hk + s * hk1;
                h(k + 1, j) = -std::conj(s) * hk + c * hk1;
            }
            int row1 = std::min(k + 2, N - 1);
            for (int i = 0; i <= row1; ++i) {
                Complex hk = h(i, k);
                Complex hk1 = h(i, k + 1);
                h(i, k)     = c * hk + std::conj(s) * hk1;
                h(i, k + 1) = -s * hk + c * hk1;
            }
            for (int i = 0; i < N; ++i) {
                Complex zk = z(i, k);
                Complex zk1 = z(i, k + 1);
                z(i, k)     = c * zk + std::conj(s) * zk1;
                z(i, k + 1) = -s * zk + c * zk1;
            }
        }
    }

    for (int j = 0; j + 1 < N; ++j)
        for (int i = j + 1; i < N; ++i)
            h(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = Complex(0.0, 0.0);
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
    if (A.isComplex()) {
        auto Hout = Value::complexMatrix(n, n, mr);
        auto Pout = Value::complexMatrix(n, n, mr);
        if (n == 0) return std::make_tuple(std::move(Pout), std::move(Hout));
        std::copy(A.complexData(), A.complexData() + n * n, Hout.complexDataMut());
        hessReduceInplace(Hout.complexDataMut(), n, Pout.complexDataMut(), mr);
        return std::make_tuple(detail::narrow_if_real(Pout, mr), detail::narrow_if_real(Hout, mr));
    }
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

namespace {

// Francis double-shift QR on an upper-Hessenberg H (n×n, column-major), with
// the orthogonal accumulator Z (pre-seeded with the Hessenberg transform P).
// On exit H is the real (quasi-upper-triangular) Schur form T — 1×1 blocks for
// real eigenvalues, 2×2 blocks for complex-conjugate pairs — and A == Z·T·Zᵀ.
// Classical bulge-chasing implicit double-shift QR (Golub & Van Loan §7.5).
void francisSchur(double *H, double *Z, std::size_t n)
{
    if (n < 3) return;   // ≤2×2 is already (quasi-)triangular
    auto h = [&](std::size_t i, std::size_t j) -> double & { return H[i + j * n]; };
    auto z = [&](std::size_t i, std::size_t j) -> double & { return Z[i + j * n]; };
    const double eps = std::numeric_limits<double>::epsilon();
    const int    N   = static_cast<int>(n);

    int p = N - 1;          // bottom of the active (un-deflated) block
    int iter = 0;
    while (p >= 0) {
        // Deflation: smallest l so the active block is H[l..p, l..p].
        int l = p;
        while (l > 0) {
            double s = std::fabs(h(l - 1, l - 1)) + std::fabs(h(l, l));
            if (s == 0.0) s = 1.0;
            if (std::fabs(h(l, l - 1)) <= eps * s) { h(l, l - 1) = 0.0; break; }
            --l;
        }
        if (l == p)       { p -= 1; iter = 0; continue; }   // 1×1 converged
        if (l == p - 1)   { p -= 2; iter = 0; continue; }   // 2×2 converged
        if (++iter > 200) break;                             // give up (non-convergence)

        // Double shift from the trailing 2×2: trace s, determinant t.
        double s = h(p - 1, p - 1) + h(p, p);
        double t = h(p - 1, p - 1) * h(p, p) - h(p - 1, p) * h(p, p - 1);
        if (iter % 30 == 0) {           // exceptional shift to break cycles
            const double e = std::fabs(h(p, p - 1)) + std::fabs(h(p - 1, p - 2));
            s = 1.5 * e;
            t = e * e;
        }
        // First column of (H² − sH + tI) on the active block — bulge seed.
        double x = h(l, l) * h(l, l) + h(l, l + 1) * h(l + 1, l) - s * h(l, l) + t;
        double y = h(l + 1, l) * (h(l, l) + h(l + 1, l + 1) - s);
        double w = (l + 2 <= p) ? h(l + 1, l) * h(l + 2, l + 1) : 0.0;

        for (int k = l; k <= p - 1; ++k) {
            const int r = (k <= p - 2) ? 3 : 2;     // reflector size (3 mid-chase, 2 at the end)
            // Householder that zeroes (y[,w]) below x.
            double scale = std::fabs(x) + std::fabs(y) + (r == 3 ? std::fabs(w) : 0.0);
            if (scale == 0.0) continue;
            double xs = x / scale, ys = y / scale, ws = (r == 3 ? w / scale : 0.0);
            double alpha = std::sqrt(xs * xs + ys * ys + ws * ws);
            if (xs < 0.0) alpha = -alpha;
            double v0 = xs + alpha, v1 = ys, v2 = ws;
            const double vnorm2 = v0 * v0 + v1 * v1 + v2 * v2;
            if (vnorm2 == 0.0) continue;
            const double tau = 2.0 / vnorm2;

            const int col0 = (k > l) ? k - 1 : l;   // first affected column
            // Left-apply Hᵤ to rows k..k+r-1 of H (cols col0..N-1).
            for (int j = col0; j < N; ++j) {
                double d = v0 * h(k, j) + v1 * h(k + 1, j) + (r == 3 ? v2 * h(k + 2, j) : 0.0);
                d *= tau;
                h(k, j)     -= d * v0;
                h(k + 1, j) -= d * v1;
                if (r == 3) h(k + 2, j) -= d * v2;
            }
            // Right-apply Hᵤ to cols k..k+r-1 of H (rows 0..min(k+r,p)).
            const int rowEnd = std::min(k + r, p);
            for (int i = 0; i <= rowEnd; ++i) {
                double d = v0 * h(i, k) + v1 * h(i, k + 1) + (r == 3 ? v2 * h(i, k + 2) : 0.0);
                d *= tau;
                h(i, k)     -= d * v0;
                h(i, k + 1) -= d * v1;
                if (r == 3) h(i, k + 2) -= d * v2;
            }
            // Accumulate into Z (cols k..k+r-1, all rows).
            for (int i = 0; i < N; ++i) {
                double d = v0 * z(i, k) + v1 * z(i, k + 1) + (r == 3 ? v2 * z(i, k + 2) : 0.0);
                d *= tau;
                z(i, k)     -= d * v0;
                z(i, k + 1) -= d * v1;
                if (r == 3) z(i, k + 2) -= d * v2;
            }
            // Next bulge element.
            x = h(k + 1, k);
            y = h(k + 2, k);
            w = (k + 3 <= p) ? h(k + 3, k) : 0.0;
        }
    }
    // Zero the numerical dust strictly below the quasi-triangular structure.
    for (int j = 0; j + 2 < N; ++j)
        for (int i = j + 2; i < N; ++i)
            h(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = 0.0;
}

// Standardize every 2×2 diagonal block of the (quasi-triangular) Schur form:
// a block with REAL eigenvalues is triangularized (subdiagonal → 0) via a Givens
// similarity (accumulated into Z); a complex-conjugate pair is left as a valid
// 2×2 block. LAPACK dlanv2 logic. Also covers the n==2 input that francisSchur
// skips. H/Z column-major, n×n.
void standardizeSchur2x2(double *H, double *Z, std::size_t n)
{
    if (n < 2) return;
    auto h = [&](std::size_t i, std::size_t j) -> double & { return H[i + j * n]; };
    auto z = [&](std::size_t i, std::size_t j) -> double & { return Z[i + j * n]; };
    const int N = static_cast<int>(n);

    for (int k = 0; k + 1 < N; ++k) {
        if (k + 1 < N && h(k + 1, k) == 0.0) continue;      // 1×1 block
        if (k + 2 < N && h(k + 2, k + 1) != 0.0) continue;  // part of a larger active region (shouldn't happen post-QR)

        double a = h(k, k), b = h(k, k + 1), c = h(k + 1, k), d = h(k + 1, k + 1);
        if (c == 0.0) continue;

        double cs = 1.0, sn = 0.0;
        const double p = 0.5 * (a - d);
        const double bcmax = std::max(std::fabs(b), std::fabs(c));
        const double bcmis = std::min(std::fabs(b), std::fabs(c)) *
                             (b >= 0.0 ? 1.0 : -1.0) * (c >= 0.0 ? 1.0 : -1.0);
        const double scale = std::max(std::fabs(p), bcmax);
        double zz = (p / scale) * p + (bcmax / scale) * bcmis;

        if (zz >= 0.0) {
            // Real eigenvalues → triangularize.
            double zr = p + (p >= 0.0 ? 1.0 : -1.0) * std::sqrt(scale) * std::sqrt(zz);
            a = d + zr;
            d -= (bcmax / zr) * bcmis;
            const double tau = std::hypot(c, zr);
            cs = zr / tau;
            sn = c / tau;
            b = b - c;
            c = 0.0;
            h(k, k) = a; h(k, k + 1) = b; h(k + 1, k) = 0.0; h(k + 1, k + 1) = d;
            // Apply the rotation G = [cs sn; -sn cs] as a similarity to the rest.
            for (int j = k + 2; j < N; ++j) {       // rows k,k+1 (cols right of the block)
                const double t1 = h(k, j), t2 = h(k + 1, j);
                h(k, j)     =  cs * t1 + sn * t2;
                h(k + 1, j) = -sn * t1 + cs * t2;
            }
            for (int i = 0; i < k; ++i) {           // cols k,k+1 (rows above the block)
                const double t1 = h(i, k), t2 = h(i, k + 1);
                h(i, k)     =  cs * t1 + sn * t2;
                h(i, k + 1) = -sn * t1 + cs * t2;
            }
            for (int i = 0; i < N; ++i) {           // accumulate into Z
                const double t1 = z(i, k), t2 = z(i, k + 1);
                z(i, k)     =  cs * t1 + sn * t2;
                z(i, k + 1) = -sn * t1 + cs * t2;
            }
        }
        // Complex pair (zz<0): leave the 2×2 block as the real Schur form.
        ++k;   // skip the partner row
    }
}

} // anonymous namespace

// General (nonsymmetric) real/complex Schur: A = U·T·Uᵀ, U unitary, T upper-triangular.
std::tuple<Value, Value>
schur_general(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(0));
    if (A.isComplex()) {
        auto U = Value::complexMatrix(n, n, mr);
        auto T = Value::complexMatrix(n, n, mr);
        if (n == 0) return std::make_tuple(std::move(U), std::move(T));
        std::copy(A.complexData(), A.complexData() + n * n, T.complexDataMut());
        hessReduceInplace(T.complexDataMut(), n, U.complexDataMut(), mr);
        complexSchurQR(T.complexDataMut(), U.complexDataMut(), n);
        return std::make_tuple(detail::narrow_if_real(U, mr), detail::narrow_if_real(T, mr));
    }

    auto U = Value::matrix(n, n, ValueType::DOUBLE, mr);   // Schur vectors (== P·Q)
    auto T = Value::matrix(n, n, ValueType::DOUBLE, mr);   // real Schur form
    if (n == 0) return std::make_tuple(std::move(U), std::move(T));
    std::copy(A.doubleData(), A.doubleData() + n * n, T.doubleDataMut());
    hessReduceInplace(T.doubleDataMut(), n, U.doubleDataMut(), mr);
    francisSchur(T.doubleDataMut(), U.doubleDataMut(), n);
    standardizeSchur2x2(T.doubleDataMut(), U.doubleDataMut(), n);
    return std::make_tuple(std::move(U), std::move(T));
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

} // namespace numkit::linalg
