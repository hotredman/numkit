// toolboxes/control/src/riccati/riccati.cpp
//
// Algebraic Riccati solvers (care / dare) by the matrix sign-function
// method. See riccati.hpp for the equations.
//
// Why the sign function (and not Schur ordering): the stabilizing
// solution is read off the stable invariant subspace of an embedding
// matrix. The sign function S = sign(M) is a projector onto that
// subspace — sign(M) has eigenvalues ±1 splitting the spectrum by the
// sign of the real part — so range(I − S) gives the stable basis
// directly, with no eigenvalue reordering. The Newton iteration
//     Z ← ½(cZ + (cZ)⁻¹),   c = √(‖Z⁻¹‖_F / ‖Z‖_F)
// converges quadratically to sign(Z); the norm scaling c is Higham's
// standard accelerator (the fixed point is unchanged, only the path).
//
// Block read-off (identical for both equations): with S partitioned
//     S = [Z11 Z12; Z21 Z22]   (each block n×n),
//     W1 = [Z12; Z22 + I],  W2 = −[Z11 + I; Z21],
//     X  = W1 \ W2   (2n×n least squares → normal equations),  X ← ½(X+Xᵀ).
//
// care embeds the Hamiltonian H directly. dare has no Hamiltonian, but
// the symplectic matrix's stable eigenvalues sit inside the unit disk;
// a Cayley transform C = (S−I)(S+I)⁻¹ maps |λ|<1 → Re<0, after which the
// same sign machinery applies.
//
// Engine-free compute TU: Value substrate + Error + math::roots only.
// The care / dare builtins (CallContext wrappers) live in riccati_reg.cpp.

#include <numkit/control/riccati/riccati.hpp>
#include <numkit/control/internal/numerics.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/math/poly/polynomials.hpp>   // numkit::math::roots

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::control {

namespace {

using Mat = internal::Mat;   // column-major flat std::vector<double>
using internal::solveInPlace;
using internal::charPoly;

// --- small column-major matrix helpers ------------------------------

Mat readMat(const Value &v, size_t r, size_t c) {
    Mat M(r * c, 0.0);
    for (size_t i = 0; i < r * c; ++i) M[i] = v.elemAsDouble(i);
    return M;
}

Value matFromVec(size_t r, size_t c, const Mat &v, std::pmr::memory_resource *mr) {
    Value m = Value::matrix(r, c, ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), m.doubleDataMut());
    return m;
}

// C = A·B with A: ra×ca, B: rb×cb (ca == rb), result ra×cb, column-major.
Mat matmul(const Mat &A, size_t ra, size_t ca,
           const Mat &B, size_t rb, size_t cb) {
    (void)rb;  // ca == rb by contract
    Mat C(ra * cb, 0.0);
    for (size_t j = 0; j < cb; ++j)
        for (size_t k = 0; k < ca; ++k) {
            const double b = B[j * ca + k];          // B[k, j]
            if (b == 0.0) continue;
            for (size_t i = 0; i < ra; ++i)
                C[j * ra + i] += A[k * ra + i] * b;   // A[i, k] · B[k, j]
        }
    return C;
}

Mat transpose(const Mat &A, size_t r, size_t c) {
    Mat T(r * c, 0.0);
    for (size_t j = 0; j < c; ++j)
        for (size_t i = 0; i < r; ++i)
            T[i * c + j] = A[j * r + i];   // T[j, i] = A[i, j]
    return T;
}

Mat identity(size_t n) {
    Mat I(n * n, 0.0);
    for (size_t i = 0; i < n; ++i) I[i * n + i] = 1.0;
    return I;
}

// Inverse of an n×n matrix via LU + identity RHS. Returns false if singular.
bool inverse(const Mat &A, size_t n, Mat &out) {
    Mat lu = A;            // solveInPlace overwrites
    out = identity(n);
    return solveInPlace(lu, out, n, n);
}

double frobNorm(const Mat &A) {
    double s = 0.0;
    for (double x : A) s += x * x;
    return std::sqrt(s);
}

// Matrix sign function via scaled Newton (Higham). Converges quadratically
// for a matrix with no purely-imaginary eigenvalues (true for the
// Hamiltonian / Cayley-transformed symplectic of a well-posed Riccati).
bool matsign(Mat Z, size_t k, Mat &out) {
    const double tol = 1e-14;
    for (int iter = 0; iter < 200; ++iter) {
        Mat Zi;
        if (!inverse(Z, k, Zi)) return false;
        const double c = std::sqrt(frobNorm(Zi) / std::max(frobNorm(Z), 1e-300));
        Mat Zn(k * k, 0.0);
        double diff = 0.0, scale = 0.0;
        for (size_t i = 0; i < k * k; ++i) {
            Zn[i] = 0.5 * (c * Z[i] + Zi[i] / c);
            const double d = Zn[i] - Z[i];
            diff += d * d;
            scale += Zn[i] * Zn[i];
        }
        Z.swap(Zn);
        if (std::sqrt(diff) <= tol * std::sqrt(std::max(scale, 1e-300))) break;
    }
    out.swap(Z);
    return true;
}

// Read the stabilizing X out of a 2n×2n sign matrix S (block layout above),
// then symmetrize. Solves the 2n×n least-squares system W1·X = W2 via the
// normal equations (W1ᵀW1)·X = W1ᵀW2 (n×n, well-conditioned because the
// columns of W1 span the stable subspace).
bool subspaceToX(const Mat &S, size_t n, Mat &X) {
    const size_t k = 2 * n;
    Mat W1(k * n, 0.0), W2(k * n, 0.0);
    for (size_t j = 0; j < n; ++j) {
        for (size_t i = 0; i < n; ++i) {
            const double z11 = S[(j) * k + (i)];
            const double z12 = S[(n + j) * k + (i)];
            const double z21 = S[(j) * k + (n + i)];
            const double z22 = S[(n + j) * k + (n + i)];
            // W1 = [Z12; Z22 + I]
            W1[j * k + i]       = z12;
            W1[j * k + (n + i)] = z22 + (i == j ? 1.0 : 0.0);
            // W2 = -[Z11 + I; Z21]
            W2[j * k + i]       = -(z11 + (i == j ? 1.0 : 0.0));
            W2[j * k + (n + i)] = -z21;
        }
    }
    Mat W1t = transpose(W1, k, n);          // n×2n
    Mat A = matmul(W1t, n, k, W1, k, n);    // n×n
    Mat Bn = matmul(W1t, n, k, W2, k, n);   // n×n
    if (!solveInPlace(A, Bn, n, n)) return false;
    // symmetrize
    X.assign(n * n, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            X[j * n + i] = 0.5 * (Bn[j * n + i] + Bn[i * n + j]);
    return true;
}

// Closed-loop eigenvalues eig(A − B·G) as a (possibly complex) column Value.
Value closedLoopEig(const Mat &A, const Mat &B, const Mat &G,
                    size_t n, size_t m, std::pmr::memory_resource *mr) {
    Mat BG = matmul(B, n, m, G, m, n);   // n×n
    Mat Acl(n * n, 0.0);
    for (size_t i = 0; i < n * n; ++i) Acl[i] = A[i] - BG[i];
    auto cp = charPoly(Acl, n);          // [1, c1, …, cn]
    Value row = Value::matrix(1, cp.size(), ValueType::DOUBLE, mr);
    if (!cp.empty()) std::copy(cp.begin(), cp.end(), row.doubleDataMut());
    return numkit::math::roots(row, mr);
}

void checkShapes(const Value &A, const Value &B, const Value &Q,
                 const Value &R, const char *name,
                 size_t &n, size_t &m) {
    if (A.dims().rows() != A.dims().cols())
        throw Error(std::string(name) + ": A must be square",
                    0, 0, name, "", "numkit:care:A");
    n = A.dims().rows();
    m = B.dims().cols();
    if (B.dims().rows() != n)
        throw Error(std::string(name) + ": B must have the same row count as A",
                    0, 0, name, "", "numkit:care:B");
    if (Q.dims().rows() != n || Q.dims().cols() != n)
        throw Error(std::string(name) + ": Q must be n×n",
                    0, 0, name, "", "numkit:care:Q");
    if (R.dims().rows() != m || R.dims().cols() != m)
        throw Error(std::string(name) + ": R must be m×m (m = columns of B)",
                    0, 0, name, "", "numkit:care:R");
}

} // anonymous

RiccatiResult care(const Value &Av, const Value &Bv, const Value &Qv,
                   const Value &Rv, std::pmr::memory_resource *mr)
{
    size_t n = 0, m = 0;
    checkShapes(Av, Bv, Qv, Rv, "care", n, m);

    auto A = readMat(Av, n, n);
    auto B = readMat(Bv, n, m);
    auto Q = readMat(Qv, n, n);
    auto R = readMat(Rv, m, m);

    Mat Ri;
    if (!inverse(R, m, Ri))
        throw Error("care: R must be nonsingular",
                    0, 0, "care", "", "numkit:care:Rsingular");

    // G_BR = B·R⁻¹·Bᵀ   (n×n)
    Mat Bt = transpose(B, n, m);
    Mat RiBt = matmul(Ri, m, m, Bt, m, n);     // m×n
    Mat Gbr = matmul(B, n, m, RiBt, m, n);     // n×n

    // Hamiltonian H = [A, -G_BR; -Q, -Aᵀ]   (2n×2n, column-major)
    const size_t k = 2 * n;
    Mat H(k * k, 0.0);
    Mat At = transpose(A, n, n);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i) {
            H[(j) * k + (i)]         =  A[j * n + i];
            H[(n + j) * k + (i)]     = -Gbr[j * n + i];
            H[(j) * k + (n + i)]     = -Q[j * n + i];
            H[(n + j) * k + (n + i)] = -At[j * n + i];
        }

    Mat S;
    if (!matsign(H, k, S))
        throw Error("care: Hamiltonian sign iteration failed (singular embedding)",
                    0, 0, "care", "", "numkit:care:sign");

    Mat X;
    if (!subspaceToX(S, n, X))
        throw Error("care: stable-subspace solve is singular",
                    0, 0, "care", "", "numkit:care:subspace");

    // Gain G = R⁻¹·Bᵀ·X   (m×n)
    Mat BtX = matmul(Bt, m, n, X, n, n);       // m×n
    Mat G = matmul(Ri, m, m, BtX, m, n);       // m×n

    RiccatiResult out;
    out.X = matFromVec(n, n, X, mr);
    out.L = closedLoopEig(A, B, G, n, m, mr);
    out.G = matFromVec(m, n, G, mr);
    return out;
}

RiccatiResult dare(const Value &Av, const Value &Bv, const Value &Qv,
                   const Value &Rv, std::pmr::memory_resource *mr)
{
    size_t n = 0, m = 0;
    checkShapes(Av, Bv, Qv, Rv, "dare", n, m);

    auto A = readMat(Av, n, n);
    auto B = readMat(Bv, n, m);
    auto Q = readMat(Qv, n, n);
    auto R = readMat(Rv, m, m);

    Mat Ri;
    if (!inverse(R, m, Ri))
        throw Error("dare: R must be nonsingular",
                    0, 0, "dare", "", "numkit:dare:Rsingular");

    Mat Ai;
    if (!inverse(A, n, Ai))
        throw Error("dare: A must be nonsingular for the symplectic form "
                    "(singular-A path needs a QZ solver, not yet implemented)",
                    0, 0, "dare", "", "numkit:dare:Asingular");

    // G_BR = B·R⁻¹·Bᵀ   (n×n);  AiT = A⁻ᵀ
    Mat Bt = transpose(B, n, m);
    Mat RiBt = matmul(Ri, m, m, Bt, m, n);     // m×n
    Mat Gbr = matmul(B, n, m, RiBt, m, n);     // n×n
    Mat AiT = transpose(Ai, n, n);

    // Symplectic matrix
    //   Sp = [A + G_BR·A⁻ᵀ·Q,  -G_BR·A⁻ᵀ;  -A⁻ᵀ·Q,  A⁻ᵀ]   (2n×2n)
    Mat GbrAiT = matmul(Gbr, n, n, AiT, n, n); // n×n
    Mat AiTQ   = matmul(AiT, n, n, Q,  n, n);  // n×n
    Mat top    = matmul(GbrAiT, n, n, Q, n, n);// n×n  (= G_BR·A⁻ᵀ·Q)

    const size_t k = 2 * n;
    Mat Sp(k * k, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i) {
            Sp[(j) * k + (i)]         =  A[j * n + i] + top[j * n + i];
            Sp[(n + j) * k + (i)]     = -GbrAiT[j * n + i];
            Sp[(j) * k + (n + i)]     = -AiTQ[j * n + i];
            Sp[(n + j) * k + (n + i)] =  AiT[j * n + i];
        }

    // Cayley: C = (Sp − I)·(Sp + I)⁻¹   (unit disk → left half-plane)
    Mat SmI = Sp, SpI = Sp;
    for (size_t i = 0; i < k; ++i) { SmI[i * k + i] -= 1.0; SpI[i * k + i] += 1.0; }
    Mat SpIinv;
    if (!inverse(SpI, k, SpIinv))
        throw Error("dare: Cayley transform singular (Sp has an eigenvalue at −1)",
                    0, 0, "dare", "", "numkit:dare:cayley");
    Mat C = matmul(SmI, k, k, SpIinv, k, k);

    Mat S;
    if (!matsign(C, k, S))
        throw Error("dare: symplectic sign iteration failed (singular embedding)",
                    0, 0, "dare", "", "numkit:dare:sign");

    Mat X;
    if (!subspaceToX(S, n, X))
        throw Error("dare: stable-subspace solve is singular",
                    0, 0, "dare", "", "numkit:dare:subspace");

    // Gain G = (R + Bᵀ·X·B)⁻¹·(Bᵀ·X·A)   (m×n)
    Mat XB  = matmul(X, n, n, B, n, m);        // n×m
    Mat BtXB = matmul(Bt, m, n, XB, n, m);     // m×m
    Mat M(m * m, 0.0);
    for (size_t i = 0; i < m * m; ++i) M[i] = R[i] + BtXB[i];
    Mat BtX = matmul(Bt, m, n, X, n, n);       // m×n
    Mat BtXA = matmul(BtX, m, n, A, n, n);     // m×n
    if (!solveInPlace(M, BtXA, m, n))
        throw Error("dare: gain solve singular (R + BᵀXB not invertible)",
                    0, 0, "dare", "", "numkit:dare:gain");
    Mat &G = BtXA;                             // now holds the gain, m×n

    RiccatiResult out;
    out.X = matFromVec(n, n, X, mr);
    out.L = closedLoopEig(A, B, G, n, m, mr);
    out.G = matFromVec(m, n, G, mr);
    return out;
}

} // namespace numkit::control
