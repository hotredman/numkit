// libs/control/src/lyapunov/lyapunov.cpp
//
// Direct Kronecker-vectorisation Lyapunov solver. The continuous
// equation
//     A·X + X·Aᵀ + Q = 0
// vectorises to
//     (I ⊗ A  +  A ⊗ I) · vec(X) = −vec(Q),
// while the discrete version
//     A·X·Aᵀ − X + Q = 0
// becomes
//     (A ⊗ A  −  I) · vec(X) = −vec(Q).
//
// The matrices on the left are n²·n² with full-rank algebra for
// stable A; we solve them directly via partial-pivot LU. This is
// O(n⁶) work — perfectly fine up to n ≈ 32 (typical for textbook
// control problems). Bartels–Stewart is the textbook upgrade for
// larger systems but would need a Schur decomposition we don't
// have yet.

#include <numkit/control/lyapunov/lyapunov.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::control {

namespace {

using Mat = std::vector<double>;
using Vec = std::vector<double>;

// LU + back-sub on a column-major n×n matrix with `nrhs` right-hand
// side columns (held column-major in B). Same kernel that response/
// discretize/freq use.
bool solveInPlace(Mat &A, Mat &B, size_t n, size_t nrhs)
{
    for (size_t k = 0; k < n; ++k) {
        size_t pk = k;
        double bestAbs = std::abs(A[k * n + k]);
        for (size_t i = k + 1; i < n; ++i) {
            double v = std::abs(A[k * n + i]);
            if (v > bestAbs) { bestAbs = v; pk = i; }
        }
        if (bestAbs < 1e-14) return false;
        if (pk != k) {
            for (size_t j = 0; j < n; ++j)  std::swap(A[j * n + k], A[j * n + pk]);
            for (size_t j = 0; j < nrhs; ++j) std::swap(B[j * n + k], B[j * n + pk]);
        }
        const double diag = A[k * n + k];
        for (size_t i = k + 1; i < n; ++i) {
            const double f = A[k * n + i] / diag;
            A[k * n + i] = f;
            for (size_t j = k + 1; j < n; ++j)
                A[j * n + i] -= f * A[j * n + k];
            for (size_t j = 0; j < nrhs; ++j)
                B[j * n + i] -= f * B[j * n + k];
        }
    }
    for (size_t j = 0; j < nrhs; ++j) {
        for (size_t i = n; i-- > 0;) {
            double s = B[j * n + i];
            for (size_t k = i + 1; k < n; ++k) s -= A[k * n + i] * B[j * n + k];
            B[j * n + i] = s / A[i * n + i];
        }
    }
    return true;
}

Mat readMat(const Value &v, size_t r, size_t c) {
    Mat M(r * c, 0.0);
    for (size_t i = 0; i < r * c; ++i) M[i] = v.elemAsDouble(i);
    return M;
}

Value matFromVec(std::pmr::memory_resource *mr,
                 size_t r, size_t c, const Mat &v) {
    Value m = Value::matrix(r, c, ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), m.doubleDataMut());
    return m;
}

void checkABQ(const Value &A, const Value &Q, const char *name) {
    if (A.dims().rows() != A.dims().cols())
        throw Error(std::string(name) + ": A must be square",
                    0, 0, name, "", "m:lyap:A");
    if (Q.dims().rows() != A.dims().rows() ||
        Q.dims().cols() != A.dims().cols())
        throw Error(std::string(name) + ": Q must match A in shape",
                    0, 0, name, "", "m:lyap:Q");
}

// Solve a Kronecker-form linear system M·x = b, where M is n²×n²
// in column-major. Inputs are pre-built.
//
// Used by both lyap and dlyap; the only difference is how M is
// constructed.
Vec solveKron(Mat &M, Vec &rhs, size_t n2)
{
    Mat rhsMat = rhs;  // single-rhs solveInPlace expects column-major
    if (!solveInPlace(M, rhsMat, n2, 1))
        throw Error("lyap/dlyap: linear system is singular "
                    "(check that A is stable / Lyapunov equation is solvable)",
                    0, 0, "lyap", "", "m:lyap:singular");
    return rhsMat;
}

} // anonymous

Value lyap(std::pmr::memory_resource *mr, const Value &Av, const Value &Qv)
{
    checkABQ(Av, Qv, "lyap");
    const size_t n = Av.dims().rows();
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    auto A = readMat(Av, n, n);
    auto Q = readMat(Qv, n, n);

    // Build M = I⊗A + A⊗I, of size n²×n² (column-major).
    // Kronecker convention used throughout: (P⊗R)[i+np, j+nq]
    // = P[p,q] · R[i,j]. The vec(X) layout is column-major so
    // vec(X)[i + n·j] = X[i,j].
    const size_t n2 = n * n;
    Mat M(n2 * n2, 0.0);

    auto setEntry = [&](size_t row, size_t col, double v) {
        M[col * n2 + row] += v;
    };

    // (I⊗A): row = i + n·p, col = j + n·q with q == p, contribution A[i,j].
    for (size_t p = 0; p < n; ++p)
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j) {
                const size_t row = i + n * p;
                const size_t col = j + n * p;
                setEntry(row, col, A[j * n + i]);  // A[i, j] in col-major: A[j*n + i]
            }

    // (A⊗I): row = i + n·p, col = j + n·q with i == j, contribution A[p,q].
    for (size_t p = 0; p < n; ++p)
        for (size_t q = 0; q < n; ++q)
            for (size_t i = 0; i < n; ++i) {
                const size_t row = i + n * p;
                const size_t col = i + n * q;
                setEntry(row, col, A[q * n + p]);   // A[p, q]
            }

    // RHS = -vec(Q), column-major.
    Vec rhs(n2, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            rhs[i + n * j] = -Q[j * n + i];

    auto x = solveKron(M, rhs, n2);

    // Reshape vec back into n×n column-major X.
    Mat X(n2, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            X[j * n + i] = x[i + n * j];
    return matFromVec(mr, n, n, X);
}

Value dlyap(std::pmr::memory_resource *mr, const Value &Av, const Value &Qv)
{
    checkABQ(Av, Qv, "dlyap");
    const size_t n = Av.dims().rows();
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    auto A = readMat(Av, n, n);
    auto Q = readMat(Qv, n, n);

    const size_t n2 = n * n;
    Mat M(n2 * n2, 0.0);

    // (A⊗A): row = i + n·p, col = j + n·q, contribution A[p,q] · A[i,j].
    for (size_t p = 0; p < n; ++p)
        for (size_t q = 0; q < n; ++q)
            for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j < n; ++j) {
                    const size_t row = i + n * p;
                    const size_t col = j + n * q;
                    M[col * n2 + row] += A[q * n + p] * A[j * n + i];
                }
    // − I (n² × n²)
    for (size_t k = 0; k < n2; ++k) M[k * n2 + k] -= 1.0;

    Vec rhs(n2, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            rhs[i + n * j] = -Q[j * n + i];

    auto x = solveKron(M, rhs, n2);

    Mat X(n2, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            X[j * n + i] = x[i + n * j];
    return matFromVec(mr, n, n, X);
}

namespace detail {

void lyap_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("lyap: requires (A, Q)",
                    0, 0, "lyap", "", "m:lyap:nargin");
    o[0] = lyap(c.engine->resource(), a[0], a[1]);
}

void dlyap_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("dlyap: requires (A, Q)",
                    0, 0, "dlyap", "", "m:dlyap:nargin");
    o[0] = dlyap(c.engine->resource(), a[0], a[1]);
}

} // namespace detail

} // namespace numkit::control
