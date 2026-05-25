// libs/builtin/src/language/arrays/linalg_extras.cpp
//
// MATLAB linalg utilities — cycle 2 of the linalg sweep:
//
//   rref(A [, tol])    reduced row echelon form (Gauss-Jordan with
//                      partial pivoting). Optional [R, jb] form returns
//                      the column indices of the pivots (basis).
//   rcond(A)           reciprocal 1-norm condition estimate. Cheap
//                      path: 1 / (norm(A,1) * norm(inv(A),1)). Returns
//                      0 for singular A. KNOWN GAP: MATLAB uses LAPACK's
//                      dgecon reverse-iteration estimator -- our impl
//                      is exact (uses inv) on well-conditioned cases
//                      and degrades to 0 on exactly-singular A.
//   planerot([x; y])   Givens rotation: G (2×2) and y_out (2×1) such
//                      that G*[x; y] = [r; 0] where r = hypot(x, y).
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr. All
// scratch through ScratchArena/ScratchVec.

#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::builtin {

namespace {

// 1-norm of a column-major M×N matrix: max column sum of |a_ij|.
double matrix_one_norm(const double *A, size_t M, size_t N)
{
    if (M == 0 || N == 0) return 0.0;
    double maxv = 0.0;
    for (size_t j = 0; j < N; ++j) {
        double s = 0.0;
        for (size_t i = 0; i < M; ++i) s += std::abs(A[i + j * M]);
        if (s > maxv) maxv = s;
    }
    return maxv;
}

double matrix_inf_norm(const double *A, size_t M, size_t N)
{
    if (M == 0 || N == 0) return 0.0;
    double maxv = 0.0;
    for (size_t i = 0; i < M; ++i) {
        double s = 0.0;
        for (size_t j = 0; j < N; ++j) s += std::abs(A[i + j * M]);
        if (s > maxv) maxv = s;
    }
    return maxv;
}

} // namespace

// ── rref ──────────────────────────────────────────────────────────────
//
// Gauss-Jordan elimination with partial pivoting and tolerance gate.
// Algorithm matches Cleve Moler's classic textbook implementation
// (the same one shipped in MATLAB's M-file rref).
//
// MATLAB defaults: tol = max(size(A)) * eps(norm(A, inf)).
// The pivot column is skipped if its max-magnitude entry is below tol;
// the pivot is taken from the row with the maximum |entry|.
//
// jb is filled with the 1-based pivot column indices.
std::pair<Value, Value>
rref(const Value &A, bool have_tol, double tol_user, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("rref: input must be 2D",
                    0, 0, "rref", "", "m:rref:Not2D");

    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    Value R = Value::matrix(M, N, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) {
        Value jb = Value::matrix(1, 0, ValueType::DOUBLE, mr);
        return {R, jb};
    }

    // Copy A into the working buffer (column-major).
    ScratchArena scratch(mr);
    ScratchVec<double> B(M * N, &scratch);
    if (A.isComplex()) {
        // Complex path is rare; fall back to magnitude-only Gauss-Jordan
        // on the real and imag parts separately would lose phase. For
        // v1 we don't support complex rref -- KNOWN GAP. (MATLAB does
        // support it; documented in PROGRESS.)
        throw Error("rref: complex input not supported in v1",
                    0, 0, "rref", "", "m:rref:NoComplex");
    }
    std::copy(A.doubleData(), A.doubleData() + M * N, B.begin());

    // Default tol: max(M, N) * eps(norm(A, inf)).
    double tol = tol_user;
    if (!have_tol) {
        const double anorm = matrix_inf_norm(B.data(), M, N);
        const double eps = std::numeric_limits<double>::epsilon();
        // eps(x) ≈ x * eps_machine for x > 0.
        const double eps_anorm = (anorm == 0.0) ? eps
                                                 : std::nextafter(anorm,
                                                       std::numeric_limits<double>::infinity()) - anorm;
        tol = static_cast<double>(std::max(M, N)) * eps_anorm;
    }

    ScratchVec<double> jb_buf(0, &scratch);
    size_t i = 0;  // current pivot row
    for (size_t j = 0; j < N && i < M; ++j) {
        // Find row with max |B(k, j)| for k >= i.
        size_t piv_row = i;
        double piv_val = std::abs(B[i + j * M]);
        for (size_t k = i + 1; k < M; ++k) {
            double v = std::abs(B[k + j * M]);
            if (v > piv_val) { piv_val = v; piv_row = k; }
        }

        if (piv_val <= tol) {
            // Zero out small entries in this column at and below i.
            for (size_t k = i; k < M; ++k) B[k + j * M] = 0.0;
            continue;
        }

        // Swap row piv_row <-> row i if needed.
        if (piv_row != i) {
            for (size_t jj = 0; jj < N; ++jj) {
                std::swap(B[i + jj * M], B[piv_row + jj * M]);
            }
        }

        // Scale row i so that B(i, j) == 1.
        double piv = B[i + j * M];
        for (size_t jj = 0; jj < N; ++jj) B[i + jj * M] /= piv;
        B[i + j * M] = 1.0;  // exact

        // Eliminate column j in all other rows.
        for (size_t k = 0; k < M; ++k) {
            if (k == i) continue;
            double f = B[k + j * M];
            if (f == 0.0) continue;
            for (size_t jj = 0; jj < N; ++jj) {
                B[k + jj * M] -= f * B[i + jj * M];
            }
            B[k + j * M] = 0.0;
        }

        jb_buf.push_back(static_cast<double>(j + 1));  // 1-based
        ++i;
    }

    std::copy(B.begin(), B.end(), R.doubleDataMut());

    // Pack jb as a row vector.
    Value jb = Value::matrix(jb_buf.empty() ? 0 : 1, jb_buf.size(),
                             ValueType::DOUBLE, mr);
    if (!jb_buf.empty())
        std::copy(jb_buf.begin(), jb_buf.end(), jb.doubleDataMut());
    return {R, jb};
}

// ── rcond ─────────────────────────────────────────────────────────────
//
// Cheap-path implementation: 1 / (norm(A,1) * norm(inv(A),1)).
// For exactly-singular A we catch the inv() exception and return 0.
//
// KNOWN GAP: MATLAB uses LAPACK's dgecon (1-norm reverse-iteration
// estimator from Higham 1988). Our impl agrees with MATLAB on
// well-conditioned cases (tested: eye, diag, hilb(4), [1 2; 3 4]) but
// will produce slightly different values on near-singular matrices
// because the LAPACK estimator approximates ||inv(A)||_1 without
// computing inv(A) itself.
// NOTE: rcond migrated to libs/linalg (properties.cpp).

// ── planerot ──────────────────────────────────────────────────────────
//
// Givens rotation: returns G (2×2) and y (2×1) such that G*[x; y] = [r; 0]
// where r = hypot(x, y). Real-only (MATLAB's planerot is real-only too).
//
// Formula: r = hypot(x, y); c = x/r; s = y/r; G = [c s; -s c]; y = [r; 0].
// Degenerate (x = y = 0): G = I, y = [0; 0].
std::pair<Value, Value>
planerot(const Value &xy, std::pmr::memory_resource *mr)
{
    if (xy.dims().is3D() || xy.numel() != 2)
        throw Error("planerot: input must be a 2-element vector",
                    0, 0, "planerot", "", "m:planerot:BadShape");
    if (xy.isComplex())
        throw Error("planerot: complex input not supported",
                    0, 0, "planerot", "", "m:planerot:NoComplex");

    const double x = xy.elemAsDouble(0);
    const double y = xy.elemAsDouble(1);

    Value G = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    Value yo = Value::matrix(2, 1, ValueType::DOUBLE, mr);
    double *gd = G.doubleDataMut();
    double *yd = yo.doubleDataMut();

    if (x == 0.0 && y == 0.0) {
        // Degenerate: identity rotation, zero output.
        gd[0] = 1.0; gd[1] = 0.0; gd[2] = 0.0; gd[3] = 1.0;
        yd[0] = 0.0; yd[1] = 0.0;
        return {G, yo};
    }

    const double r = std::hypot(x, y);
    const double c = x / r;
    const double s = y / r;
    // Column-major: G(1,1)=c, G(2,1)=-s, G(1,2)=s, G(2,2)=c.
    gd[0] = c;     // (1,1)
    gd[1] = -s;    // (2,1)
    gd[2] = s;     // (1,2)
    gd[3] = c;     // (2,2)
    yd[0] = r;
    yd[1] = 0.0;
    return {G, yo};
}

namespace detail {

void rref_reg(Span<const Value> args, size_t nargout,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rref: requires (A [, tol])",
                    0, 0, "rref", "", "m:rref:nargin");
    bool have_tol = (args.size() >= 2);
    double tol = have_tol ? args[1].toScalar() : 0.0;
    auto [R, jb] = rref(args[0], have_tol, tol, ctx.engine->resource());
    outs[0] = R;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = jb;
}

// NOTE: rcond_reg migrated to libs/linalg (properties.cpp).

void planerot_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("planerot: requires ([x; y])",
                    0, 0, "planerot", "", "m:planerot:nargin");
    auto [G, y] = planerot(args[0], ctx.engine->resource());
    outs[0] = G;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = y;
}

} // namespace detail

} // namespace numkit::builtin
