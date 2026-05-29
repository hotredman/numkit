// libs/builtin/src/lang/arrays/matrix.cpp

#include <numkit/builtin/language/arrays/matrix.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "rows_helpers.hpp"
#include "language/operators/backends/binary_ops_loops.hpp"
#include "language/operators/la_solve.hpp"
#include "math/arithmetic/cumsum.hpp"
#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/builtin/language/arrays/manip.hpp>     // flip()

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Constructors ──────────────────────────────────────────────────────
Value zeros(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    return createMatrix({rows, cols, pages}, ValueType::DOUBLE, mr);
}

Value ones(size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    auto m = createMatrix({rows, cols, pages}, ValueType::DOUBLE, mr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

// ND overloads: caller passes a flat dim list. For nd <= 3 these just
// route to the legacy 2D/3D ctors via createMatrixND; nd > 3 hits the
// Value::matrixND ctor and the SBO Dims storage.
Value zerosND(const size_t *dims, std::size_t nDims, std::pmr::memory_resource *mr)
{
    return createMatrixND(dims, nDims, ValueType::DOUBLE, mr);
}

Value onesND(const size_t *dims, std::size_t nDims, std::pmr::memory_resource *mr)
{
    auto m = createMatrixND(dims, nDims, ValueType::DOUBLE, mr);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

Value eye(size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto m = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < std::min(rows, cols); ++i)
        m.elem(i, i) = 1.0;
    return m;
}

namespace {

// Siamese / de la Loubère method for odd N >= 3.
// Fills positions [0..N²-1] starting at (0, N/2) and stepping
// (-1, +1) mod N; on collision step (+1, 0) instead.
void magicOdd(double *p, size_t N)
{
    size_t r = 0;
    size_t c = N / 2;
    for (size_t k = 1; k <= N * N; ++k) {
        p[r * N + c] = static_cast<double>(k);
        const size_t nr = (r == 0) ? (N - 1) : (r - 1);
        const size_t nc = (c + 1) % N;
        if (p[nr * N + nc] != 0.0) {
            r = (r + 1) % N;          // collision: drop down
        } else {
            r = nr;
            c = nc;
        }
    }
}

// Doubly-even (N ≡ 0 mod 4): start with the natural 1..N² fill and
// swap each cell whose (i mod 4, j mod 4) is on either of the two
// 4×4-block diagonals.
void magicDoublyEven(double *p, size_t N)
{
    const size_t total = N * N;
    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < N; ++j) {
            const size_t k = i * N + j + 1;       // 1-based natural fill
            const size_t mi = i % 4;
            const size_t mj = j % 4;
            const bool diag = (mi == mj) || (mi + mj == 3);
            p[i * N + j] = static_cast<double>(diag ? (total + 1 - k) : k);
        }
}

// Singly-even (N ≡ 2 mod 4, N >= 6) via Strachey's method.
// This mirrors MATLAB R2025b's magic.m verbatim:
//   p = N/2;  K = (N-2)/4;
//   M = [Q  Q+2p²; Q+3p²  Q+p²]   where Q = magic(p)
//   For columns j in {0..K-1, N-K+1..N-1}:
//     swap rows r ∈ {0..p-1} with rows r+p (column-by-column).
//   Then for row mid = K (0-indexed = (p-1)/2):
//     swap (mid, 0) ↔ (mid+p, 0)   -- undo the previous swap on this cell
//     swap (mid, K) ↔ (mid+p, K)   -- and apply the strached swap instead
void magicSinglyEven(double *p, size_t N)
{
    const size_t P = N / 2;          // odd
    const size_t S = P * P;
    const size_t K = (N - 2) / 4;    // num "full" left/right cols to swap

    // Build one (P×P) odd-magic and tile into four quadrants.
    std::vector<double> sq(P * P, 0.0);
    magicOdd(sq.data(), P);

    for (size_t i = 0; i < P; ++i) {
        for (size_t j = 0; j < P; ++j) {
            const double s = sq[i * P + j];
            p[(i)     * N + (j)]     = s;                    // A (top-left)
            p[(i)     * N + (j + P)] = s + 2.0 * S;          // C (top-right)
            p[(i + P) * N + (j)]     = s + 3.0 * S;          // D (bottom-left)
            p[(i + P) * N + (j + P)] = s + 1.0 * S;          // B (bottom-right)
        }
    }

    // Bulk column swaps: leftmost K and rightmost K-1 columns get
    // top-half / bottom-half rows swapped. (For N=6, K=1 → swap col 0
    // only; right side has K-1=0 cols, none.)
    auto swapRowsAtCol = [&](size_t col) {
        for (size_t r = 0; r < P; ++r)
            std::swap(p[r * N + col], p[(r + P) * N + col]);
    };
    for (size_t c = 0; c < K; ++c)
        swapRowsAtCol(c);
    for (size_t c = N - K + 1; c < N && K > 0; ++c)
        swapRowsAtCol(c);

    // Middle-row fix: for row mid = K (0-indexed), undo column-0 swap
    // and apply column-K swap instead. (MATLAB: i=k+1, j=[1, i].)
    const size_t mid = K;
    if (mid < P) {
        std::swap(p[mid * N + 0], p[(mid + P) * N + 0]);  // undo
        std::swap(p[mid * N + K], p[(mid + P) * N + K]);  // apply
    }
}

} // anonymous namespace

// NOTE: inv / linsolve migrated to libs/linalg (properties.cpp, solvers.cpp).
//       pageinv migrated to libs/linalg (page_ops.cpp).
//       The internal helpers laSolveWrap / fillIdentity went with them.

// NOTE: SVD (one-sided Jacobi) migrated to libs/linalg/src/decompositions.cpp.
// Block below kept disabled until removal.

// NOTE: eig family, hess, schur_sym, sylvester_sym, expm, logm_sym,
//       sqrtm_sym migrated to libs/linalg/src/{eig,matrix_functions}.cpp.
//
// poly_of_matrix below is the ONE exception that stays: builtin's
// polynomials::poly_reg dispatches matrix-input to characteristic
// polynomial via this helper, so it has to live in builtin. linalg::
// eig_general_values has its own copy in linalg/src/eig.cpp — a small
// DRY violation tolerated because it's a leaf helper (no other state),
// and the alternative (publishing it from builtin to linalg via a
// public header) trades one tiny duplication for inter-lib include
// surface. TODO: consider promoting to libs/builtin/internal/ later.
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
    //   M = I, c[0] = 1
    //   for k = 1..n:
    //     M = A * M + c[k-1] * I       (NOT this form; see corrected below)
    //
    // Corrected (standard form, e.g. Faddeev 1959):
    //   M_0 = 0  ;  c[0] = 1
    //   for k = 1..n:
    //     M_k = A * (M_{k-1} + c[k-1] * I)
    //          = A * M_{k-1} + c[k-1] * A
    //     c[k] = -trace(M_k) / k
    // After the loop, c[1..n] are the coefficients (after the leading 1).

    ScratchArena scratch(mr);
    ScratchVec<double> M(n * n, 0.0, &scratch);
    ScratchVec<double> Mnext(n * n, &scratch);

    auto out = Value::matrix(1, n + 1, ValueType::DOUBLE, mr);
    double *c = out.doubleDataMut();
    c[0] = 1.0;

    const double *Adata = A.doubleData();

    for (std::size_t k = 1; k <= n; ++k) {
        // Mnext = A * (M + c[k-1] * I)
        //       = A * M + c[k-1] * A
        // First: Mnext = A * M
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

// Below: dead code, kept until cleanup. All migrated to libs/linalg.

// NOTE: pinv / orth / null_basis migrated to libs/linalg/src/
// pseudo_subspace.cpp (group 4 of the libs/linalg extraction).
// Block below kept disabled until cleanup pass.

// Extended topkrows. `cols` is the 0-indexed list of columns to sort by
// (priority order — first column is primary key, etc.). If `cols` is
// empty, sort by every column in order. `desc[j]` selects descending
// (true) vs ascending (false) for `cols[j]`. `out_idx` (optional, may
// be nullptr) receives the 0-indexed selected rows for the 2-output
// form. Stable on full ties.
Value topkrows_full(const Value &A, std::size_t k,
                    const std::vector<std::size_t> &colsIn,
                    const std::vector<std::uint8_t> &descIn,
                    std::vector<std::size_t> *out_idx,
                    std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2)
        throw Error("topkrows: input must be a 2D matrix",
                    0, 0, "topkrows", "", "numkit:topkrows:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t kk = std::min(k, m);

    // Resolve cols / desc defaults.
    std::vector<std::size_t> cols;
    std::vector<std::uint8_t> desc;
    if (colsIn.empty()) {
        cols.reserve(n);
        for (std::size_t j = 0; j < n; ++j) cols.push_back(j);
    } else {
        cols = colsIn;
    }
    if (descIn.size() == 1) {
        desc.assign(cols.size(), descIn[0]);
    } else if (descIn.empty()) {
        desc.assign(cols.size(), 1);  // default descend
    } else if (descIn.size() == cols.size()) {
        desc = descIn;
    } else {
        throw Error("topkrows: direction must be a scalar or match the "
                    "length of col",
                    0, 0, "topkrows", "", "numkit:topkrows:dirSize");
    }
    for (std::size_t j : cols)
        if (j >= n)
            throw Error("topkrows: column index out of range",
                        0, 0, "topkrows", "", "numkit:topkrows:badCol");

    ScratchArena scratch(mr);
    ScratchVec<std::size_t> idx(m, &scratch);
    for (std::size_t i = 0; i < m; ++i) idx[i] = i;

    // Use elemAsDouble for any-class input.
    auto elem = [&](std::size_t r, std::size_t c) {
        return A.elemAsDouble(r + c * m);
    };
    std::stable_sort(idx.begin(), idx.end(),
        [&](std::size_t a, std::size_t b) {
            for (std::size_t k2 = 0; k2 < cols.size(); ++k2) {
                const std::size_t j = cols[k2];
                const double va = elem(a, j);
                const double vb = elem(b, j);
                if (va == vb) continue;
                const bool a_first = desc[k2] ? (va > vb) : (va < vb);
                return a_first;
            }
            return false;  // total tie → stable order
        });

    // Preserve element class.
    auto out = (n == 0) ? Value::matrix(kk, 0, A.type(), mr)
                        : Value::matrix(kk, n, A.type(), mr);
    for (std::size_t i = 0; i < kk; ++i) {
        const std::size_t r = idx[i];
        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t src_lin = r + j * m;
            const std::size_t dst_lin = i + j * kk;
            // Class-preserving copy.
            switch (A.type()) {
                case ValueType::DOUBLE: out.doubleDataMut()[dst_lin] = A.doubleData()[src_lin]; break;
                case ValueType::SINGLE: out.singleDataMut()[dst_lin] = A.singleData()[src_lin]; break;
                case ValueType::UINT8:  out.uint8DataMut()[dst_lin]  = A.uint8Data()[src_lin];  break;
                case ValueType::UINT16: out.uint16DataMut()[dst_lin] = A.uint16Data()[src_lin]; break;
                case ValueType::UINT32: out.uint32DataMut()[dst_lin] = A.uint32Data()[src_lin]; break;
                case ValueType::UINT64: out.uint64DataMut()[dst_lin] = A.uint64Data()[src_lin]; break;
                case ValueType::INT8:   out.int8DataMut()[dst_lin]   = A.int8Data()[src_lin];   break;
                case ValueType::INT16:  out.int16DataMut()[dst_lin]  = A.int16Data()[src_lin];  break;
                case ValueType::INT32:  out.int32DataMut()[dst_lin]  = A.int32Data()[src_lin];  break;
                case ValueType::INT64:  out.int64DataMut()[dst_lin]  = A.int64Data()[src_lin];  break;
                case ValueType::LOGICAL: out.logicalDataMut()[dst_lin] = A.logicalData()[src_lin]; break;
                default:
                    out.doubleDataMut()[dst_lin] = elem(r, j);
                    break;
            }
        }
    }
    if (out_idx) {
        out_idx->resize(kk);
        for (std::size_t i = 0; i < kk; ++i) (*out_idx)[i] = idx[i];
    }
    return out;
}

Value topkrows(const Value &A, std::size_t k, std::pmr::memory_resource *mr)
{
    return topkrows_full(A, k, {}, {}, nullptr, mr);
}

// ── Toeplitz / Hankel / Vandermonde / Companion ─────────────────────

namespace {

// Extract Value as a flat double buffer in linear element order via
// elemAsDouble (handles every numeric/logical type). Returns the
// buffer; live for the lifetime of `scratch`.
ScratchVec<double> valueToScratchDoubles(const Value &v, ScratchArena &scratch)
{
    const std::size_t n = v.numel();
    ScratchVec<double> out(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

} // namespace

Value toeplitz(const Value &cV, const Value &rV, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto c = valueToScratchDoubles(cV, scratch);
    // Single-arg / Empty rV: r = c (MATLAB convention; real input).
    // ScratchVec has deleted copy ctor — duplicate c element-wise instead
    // of relying on a ternary that would force a copy.
    ScratchVec<double> r(&scratch);
    if (rV.isEmpty()) {
        r.assign(c.begin(), c.end());
    } else {
        r = valueToScratchDoubles(rV, scratch);
    }
    const std::size_t m = c.size();
    const std::size_t n = r.size();
    if (m == 0 || n == 0)
        throw Error("toeplitz: inputs must be non-empty",
                    0, 0, "toeplitz", "", "numkit:toeplitz:empty");
    auto M = Value::matrix(m, n, ValueType::DOUBLE, mr);
    // T[i, j] = c[i-j]  (i >= j)
    //        = r[j-i]  (i <  j)
    // MATLAB silently overrides r[0] with c[0] when both are given;
    // caller's r[0] is ignored.
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < m; ++i)
            M.elem(i, j) = (i >= j) ? c[i - j] : r[j - i];
    return M;
}

Value hankel(const Value &cV, const Value &rV, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto c = valueToScratchDoubles(cV, scratch);
    const std::size_t m = c.size();
    // Single-arg / Empty rV: r is all zeros, length = m
    // (anti-triangular Hankel).
    ScratchVec<double> r(&scratch);
    if (rV.isEmpty()) {
        r.assign(m, 0.0);
    } else {
        r = valueToScratchDoubles(rV, scratch);
    }
    const std::size_t n = r.size();
    if (m == 0 || n == 0)
        throw Error("hankel: inputs must be non-empty",
                    0, 0, "hankel", "", "numkit:hankel:empty");
    auto M = Value::matrix(m, n, ValueType::DOUBLE, mr);
    // H[i, j] = c[i + j]                       if i + j <  m
    //         = r[i + j - m + 1]               otherwise
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < m; ++i) {
            const size_t s = i + j;
            M.elem(i, j) = (s < m) ? c[s] : r[s - m + 1];
        }
    return M;
}

Value vander(const Value &vV, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto v = valueToScratchDoubles(vV, scratch);
    const std::size_t n = v.size();
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // V[i, j] = v[i] ^ (n - 1 - j) -- highest power on the LEFT.
    for (size_t i = 0; i < n; ++i) {
        const double x = v[i];
        M.elem(i, n - 1) = 1.0;
        for (size_t k = 1; k < n; ++k)
            M.elem(i, n - 1 - k) = M.elem(i, n - k) * x;
    }
    return M;
}

Value compan(const Value &pV, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto p = valueToScratchDoubles(pV, scratch);
    const std::size_t pn = p.size();
    if (pn < 2)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (p[0] == 0.0)
        throw Error("compan: leading coefficient must be non-zero",
                    0, 0, "compan", "", "numkit:compan:zeroLead");

    const std::size_t n = pn - 1;
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    const double inv = 1.0 / p[0];
    for (std::size_t j = 0; j < n; ++j)
        M.elem(0, j) = -p[j + 1] * inv;
    for (std::size_t i = 1; i < n; ++i)
        M.elem(i, i - 1) = 1.0;
    return M;
}

// ── Pascal / Hilbert / Wilkinson / Hadamard / Rosser ────────────────

Value pascal(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Default symmetric form: P[i, j] = C(i+j, i). Build via the
    // recurrence P[i,j] = P[i-1,j] + P[i,j-1] with P[0,*]=P[*,0]=1.
    for (size_t i = 0; i < n; ++i) {
        M.elem(i, 0) = 1.0;
        M.elem(0, i) = 1.0;
    }
    for (size_t i = 1; i < n; ++i)
        for (size_t j = 1; j < n; ++j)
            M.elem(i, j) = M.elem(i - 1, j) + M.elem(i, j - 1);
    return M;
}

Value hilb(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            M.elem(i, j) = 1.0 / static_cast<double>(i + j + 1);
    return M;
}

Value invhilb(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Closed-form (1-indexed): H⁻¹[i,j] =
    //   (-1)^(i+j) * (i+j-1) * C(n+i-1, n-j) * C(n+j-1, n-i) * C(i+j-2, i-1)²
    // Computed via long-double to delay overflow on n ≈ 13.
    auto binom = [](long n_, long k_) -> long double {
        if (k_ < 0 || k_ > n_) return 0.0L;
        if (k_ > n_ - k_) k_ = n_ - k_;
        long double r = 1.0L;
        for (long t = 1; t <= k_; ++t)
            r = r * static_cast<long double>(n_ - t + 1) / static_cast<long double>(t);
        return r;
    };
    for (size_t i0 = 0; i0 < n; ++i0)
        for (size_t j0 = 0; j0 < n; ++j0) {
            const long i = static_cast<long>(i0 + 1);
            const long j = static_cast<long>(j0 + 1);
            const long N = static_cast<long>(n);
            const long sgn = ((i + j) % 2 == 0) ? 1 : -1;
            const long double v =
                static_cast<long double>(sgn) *
                static_cast<long double>(i + j - 1) *
                binom(N + i - 1, N - j) *
                binom(N + j - 1, N - i) *
                binom(i + j - 2, i - 1) *
                binom(i + j - 2, i - 1);
            M.elem(i0, j0) = static_cast<double>(v);
        }
    return M;
}

Value wilkinson(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Diagonal: |(1:n) - (n+1)/2|; subdiagonal/superdiagonal: ones.
    const double mid = (static_cast<double>(n) + 1.0) / 2.0;
    for (size_t i = 0; i < n; ++i) {
        M.elem(i, i) = std::abs(static_cast<double>(i + 1) - mid);
        if (i > 0) {
            M.elem(i, i - 1) = 1.0;
            M.elem(i - 1, i) = 1.0;
        }
    }
    return M;
}

Value hadamard(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (n == 1) {
        auto M = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        M.elem(0, 0) = 1.0;
        return M;
    }
    // Verify n is a power of 2 (Sylvester only). 12·2^k and 20·2^k
    // are valid MATLAB orders too -- those use Paley I/II constructions
    // and are deferred (see header).
    if ((n & (n - 1)) != 0)
        throw Error("hadamard: only powers of 2 are supported in this revision (12·2^k and 20·2^k via Paley are deferred)",
                    0, 0, "hadamard", "", "numkit:hadamard:badN");

    auto M = Value::matrix(n, n, ValueType::DOUBLE, mr);
    // Sylvester recursion: H_1 = [1]; H_{2k} = [Hk Hk; Hk -Hk].
    M.elem(0, 0) = 1.0;
    for (size_t k = 1; k < n; k <<= 1) {
        // Quadrant copies for size doubling from k → 2k.
        for (size_t i = 0; i < k; ++i)
            for (size_t j = 0; j < k; ++j) {
                const double v = M.elem(i, j);
                M.elem(i,     j + k) =  v;
                M.elem(i + k, j)     =  v;
                M.elem(i + k, j + k) = -v;
            }
    }
    return M;
}

Value rosser(std::pmr::memory_resource *mr)
{
    // Hardcoded 8×8 Rosser test matrix (MATLAB R2025b: rosser()).
    static constexpr double R[64] = {
         611,   196, -192,  407,   -8,  -52,  -49,   29,
         196,   899,  113, -192,  -71,  -43,   -8,  -44,
        -192,   113,  899,  196,   61,   49,    8,   52,
         407,  -192,  196,  611,    8,   44,   59,  -23,
          -8,   -71,   61,    8,  411, -599,  208,  208,
         -52,   -43,   49,   44, -599,  411,  208,  208,
         -49,    -8,    8,   59,  208,  208,   99, -911,
          29,   -44,   52,  -23,  208,  208, -911,   99
    };
    auto M = Value::matrix(8, 8, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < 8; ++i)
        for (size_t j = 0; j < 8; ++j)
            M.elem(i, j) = R[i * 8 + j];
    return M;
}

Value magic(size_t N, std::pmr::memory_resource *mr)
{
    if (N == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (N == 1) {
        auto m = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        m.doubleDataMut()[0] = 1.0;
        return m;
    }
    if (N == 2) {
        // MATLAB's magic(2) is conventional [1 3; 4 2] (not strictly magic).
        auto m = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        double *p = m.doubleDataMut();
        // column-major layout: column 0 = {1, 4}, column 1 = {3, 2}
        p[0] = 1.0; p[1] = 4.0; p[2] = 3.0; p[3] = 2.0;
        return m;
    }

    auto m = Value::matrix(N, N, ValueType::DOUBLE, mr);
    // Build into a row-major scratch then transpose into column-major
    // storage. Algorithms above are written in row-major for clarity.
    std::vector<double> buf(N * N, 0.0);
    if (N % 2 == 1) {
        magicOdd(buf.data(), N);
    } else if (N % 4 == 0) {
        magicDoublyEven(buf.data(), N);
    } else {
        magicSinglyEven(buf.data(), N);
    }
    // Row-major buf[i*N + j] → column-major out[j*N + i]
    double *out = m.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < N; ++j)
            out[j * N + i] = buf[i * N + j];
    return m;
}

// ── Shape queries ────────────────────────────────────────────────────
Value size(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dims = x.dims();
    // Output ndim: at least 2 (MATLAB convention — a row vector reports
    // [1, n], not [n]). Otherwise the actual rank, including any extra
    // dims past 3.
    const int n = std::max(2, dims.ndim());
    auto sv = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *out = sv.doubleDataMut();
    for (int i = 0; i < n; ++i)
        out[i] = static_cast<double>(dims.dim(i));
    return sv;
}

Value size(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(x.dims().dimSize(dim - 1)), mr);
}

std::tuple<Value, Value> sizePair(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dims = x.dims();
    return std::make_tuple(
        Value::scalar(static_cast<double>(dims.rows()), mr),
        Value::scalar(static_cast<double>(dims.cols()), mr));
}

Value length(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isEmpty() || x.numel() == 0)
        return Value::scalar(0.0, mr);
    const auto &dims = x.dims();
    const double len = static_cast<double>(std::max({dims.rows(), dims.cols(), dims.pages()}));
    return Value::scalar(len, mr);
}

Value numel(const Value &x, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(x.numel()), mr);
}

Value ndims(const Value &x, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(x.dims().ndims()), mr);
}

// ── Shape transformations ────────────────────────────────────────────
Value reshape(const Value &x, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    const size_t newNumel = rows * cols * (pages == 0 ? 1 : pages);
    if (newNumel != x.numel())
        throw Error("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "numkit:reshape:elementCountMismatch");

    DimsArg d{rows, cols, pages};

    // CELL and STRING store element-wise, not in the raw buffer — memcpy
    // wouldn't copy Value members.
    if (x.type() == ValueType::CELL || x.type() == ValueType::STRING) {
        const bool is3D = d.pages > 0;
        Value r = (x.type() == ValueType::CELL)
            ? (is3D ? Value::cell3D(d.rows, d.cols, d.pages)
                    : Value::cell(d.rows, d.cols))
            : (is3D ? Value::stringArray3D(d.rows, d.cols, d.pages)
                    : Value::stringArray(d.rows, d.cols));
        auto &src = x.cellDataVec();
        auto &dst = r.cellDataVec();
        for (size_t i = 0; i < src.size() && i < dst.size(); ++i)
            dst[i] = src[i];
        return r;
    }

    auto r = createMatrix(d, x.type(), mr);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

// ND reshape. Same elem-count check, then route to matrixND for nd > 3.
// CELL/STRING ND not supported yet (matches the 2D/3D behaviour: only
// CELL/STRING currently handles 2D and 3D shapes via cell3D/stringArray3D).
Value reshapeND(const Value &x, Span<const size_t> dims, std::pmr::memory_resource *mr)
{
    const std::size_t nDims = dims.size();
    size_t newNumel = 1;
    for (std::size_t i = 0; i < nDims; ++i) newNumel *= dims[i];
    if (newNumel != x.numel())
        throw Error("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "numkit:reshape:elementCountMismatch");

    if (x.type() == ValueType::CELL || x.type() == ValueType::STRING) {
        if (nDims > 3)
            throw Error("reshape: ND CELL/STRING (>3) not yet supported",
                         0, 0, "reshape", "", "numkit:reshape:cellND");
        // Fall through to legacy path for 2D / 3D cell.
        const size_t r = nDims > 0 ? dims[0] : 1;
        const size_t c = nDims > 1 ? dims[1] : 1;
        const size_t p = nDims > 2 ? dims[2] : 0;
        return reshape(x, r, c, p, mr);
    }

    auto r = createMatrixND(dims.data(), nDims, x.type(), mr);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

Value transpose(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.dims().is3D())
        throw Error("transpose is not defined for N-D arrays",
                     0, 0, "transpose", "", "numkit:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();
    auto r = Value::matrix(cols, rows, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            r.elem(j, i) = x(i, j);
    return r;
}

// ── pagetranspose / pagectranspose ───────────────────────────────────
namespace {

// Per-page transpose helper. `conjugate` flips the sign of imaginary
// parts when input is COMPLEX. For DOUBLE / SINGLE inputs the flag is
// ignored at the element level (no-op).
template <typename T>
Value pageTransposeT(const Value &x, ValueType ty, bool conjugate, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    const size_t M = d.rows(), N = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1u;

    auto out = (P == 1u)
        ? Value::matrix(N, M, ty, mr)
        : Value::matrix3d(N, M, P, ty, mr);

    const T *src = static_cast<const T *>(x.rawData());
    T *dst       = static_cast<T *>(out.rawDataMut());
    const size_t pageInElems  = M * N;
    const size_t pageOutElems = N * M;

    for (size_t p = 0; p < P; ++p) {
        const T *sp = src + p * pageInElems;
        T *dp       = dst + p * pageOutElems;
        for (size_t j = 0; j < N; ++j) {
            for (size_t i = 0; i < M; ++i) {
                if constexpr (std::is_same_v<T, Complex>) {
                    Complex v = sp[j * M + i];
                    dp[i * N + j] = conjugate ? std::conj(v) : v;
                } else {
                    (void)conjugate;
                    dp[i * N + j] = sp[j * M + i];
                }
            }
        }
    }
    return out;
}

Value pageTransposeAny(const Value &x, bool conjugate, std::pmr::memory_resource *mr)
{
    switch (x.type()) {
    case ValueType::DOUBLE:  return pageTransposeT<double>(x, ValueType::DOUBLE, conjugate, mr);
    case ValueType::SINGLE:  return pageTransposeT<float>(x, ValueType::SINGLE, conjugate, mr);
    case ValueType::COMPLEX: return pageTransposeT<Complex>(x, ValueType::COMPLEX, conjugate, mr);
    case ValueType::INT8:    return pageTransposeT<int8_t>(x, ValueType::INT8, conjugate, mr);
    case ValueType::INT16:   return pageTransposeT<int16_t>(x, ValueType::INT16, conjugate, mr);
    case ValueType::INT32:   return pageTransposeT<int32_t>(x, ValueType::INT32, conjugate, mr);
    case ValueType::INT64:   return pageTransposeT<int64_t>(x, ValueType::INT64, conjugate, mr);
    case ValueType::UINT8:   return pageTransposeT<uint8_t>(x, ValueType::UINT8, conjugate, mr);
    case ValueType::UINT16:  return pageTransposeT<uint16_t>(x, ValueType::UINT16, conjugate, mr);
    case ValueType::UINT32:  return pageTransposeT<uint32_t>(x, ValueType::UINT32, conjugate, mr);
    case ValueType::UINT64:  return pageTransposeT<uint64_t>(x, ValueType::UINT64, conjugate, mr);
    case ValueType::LOGICAL: return pageTransposeT<uint8_t>(x, ValueType::LOGICAL, conjugate, mr);
    default:
        throw Error("pagetranspose: unsupported input type",
                     0, 0, "pagetranspose", "", "numkit:pagetranspose:badType");
    }
}

} // namespace

Value pagetranspose(const Value &x, std::pmr::memory_resource *mr)
{
    return pageTransposeAny(x, /*conjugate=*/false, mr);
}

Value pagectranspose(const Value &x, std::pmr::memory_resource *mr)
{
    return pageTransposeAny(x, /*conjugate=*/true, mr);
}

// ── sphere / cylinder / ellipsoid ───────────────────────────────────
Surface3 sphere(size_t n, std::pmr::memory_resource *mr)
{
    // Match MATLAB's parametrisation exactly:
    //   theta = (-n:2:n)/n * pi          → linspace(-π,  π,  n+1)
    //   phi   = (-n:2:n)/n * pi/2        → linspace(-π/2, π/2, n+1)
    //   cosphi(end-points) := 0          (clamp at the poles)
    //   sintheta(end-points) := 0        (clamp θ at ±π)
    //   X = cos(phi) * cos(theta)
    //   Y = cos(phi) * sin(theta)
    //   Z = sin(phi) * ones(1, n+1)
    constexpr double kPi = 3.14159265358979323846;
    const size_t m = n + 1;
    auto X = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(m, m, ValueType::DOUBLE, mr);
    auto Z = Value::matrix(m, m, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    double *yd = Y.doubleDataMut();
    double *zd = Z.doubleDataMut();

    if (n == 0) return { std::move(X), std::move(Y), std::move(Z) };

    std::vector<double> cosPhi(m), sinPhi(m);
    for (size_t i = 0; i < m; ++i) {
        const double phi = (2.0 * static_cast<double>(i) - static_cast<double>(n))
                              / static_cast<double>(n) * (kPi / 2.0);
        cosPhi[i] = std::cos(phi);
        sinPhi[i] = std::sin(phi);
    }
    cosPhi[0] = 0.0;
    cosPhi[m - 1] = 0.0;

    std::vector<double> cosTh(m), sinTh(m);
    for (size_t j = 0; j < m; ++j) {
        const double theta = (2.0 * static_cast<double>(j) - static_cast<double>(n))
                                / static_cast<double>(n) * kPi;
        cosTh[j] = std::cos(theta);
        sinTh[j] = std::sin(theta);
    }
    sinTh[0] = 0.0;
    sinTh[m - 1] = 0.0;

    for (size_t j = 0; j < m; ++j) {
        for (size_t i = 0; i < m; ++i) {
            const size_t k = j * m + i;
            xd[k] = cosPhi[i] * cosTh[j];
            yd[k] = cosPhi[i] * sinTh[j];
            zd[k] = sinPhi[i];
        }
    }
    return { std::move(X), std::move(Y), std::move(Z) };
}

Surface3 cylinder(const Value &R, size_t n, std::pmr::memory_resource *mr)
{
    // Profile R is a 1-D vector of radii along z. Output is length(R) ×
    // (n+1). z is linspace(0, 1, length(R)) repeated across columns.
    constexpr double kPi = 3.14159265358979323846;
    const size_t rows = R.numel();
    if (rows == 0) {
        auto Z = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return { Z, Z, Z };
    }

    const size_t cols = n + 1;
    auto X = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    auto Z = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    double *yd = Y.doubleDataMut();
    double *zd = Z.doubleDataMut();

    const double *Rd = R.doubleData();
    for (size_t j = 0; j < cols; ++j) {
        const double theta = 2.0 * kPi * static_cast<double>(j) / static_cast<double>(n);
        const double ct = std::cos(theta), st = std::sin(theta);
        for (size_t i = 0; i < rows; ++i) {
            const size_t k = j * rows + i;
            xd[k] = Rd[i] * ct;
            yd[k] = Rd[i] * st;
            zd[k] = (rows == 1) ? 0.0
                                 : static_cast<double>(i) / static_cast<double>(rows - 1);
        }
    }
    return { std::move(X), std::move(Y), std::move(Z) };
}

Surface3 ellipsoid(double xc, double yc, double zc, double xr, double yr, double zr, size_t n, std::pmr::memory_resource *mr)
{
    // Same parametrisation as sphere, scaled by (xr, yr, zr) and shifted.
    auto sph = sphere(n, mr);
    const size_t total = sph.X.numel();
    double *xd = sph.X.doubleDataMut();
    double *yd = sph.Y.doubleDataMut();
    double *zd = sph.Z.doubleDataMut();
    for (size_t k = 0; k < total; ++k) {
        xd[k] = xc + xr * xd[k];
        yd[k] = yc + yr * yd[k];
        zd[k] = zc + zr * zd[k];
    }
    return sph;
}

// ── peaks ────────────────────────────────────────────────────────────
Value peaks(size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    auto Z = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *zd = Z.doubleDataMut();

    // x, y = linspace(-3, 3, n) on a meshgrid: X varies along columns,
    // Y along rows (so row i → y, col j → x — matches MATLAB peaks).
    const double step = (n > 1) ? 6.0 / static_cast<double>(n - 1) : 0.0;
    for (size_t j = 0; j < n; ++j) {  // column index → x axis
        const double x = (n > 1) ? -3.0 + step * static_cast<double>(j) : 0.0;
        for (size_t i = 0; i < n; ++i) {  // row index → y axis
            const double y = (n > 1) ? -3.0 + step * static_cast<double>(i) : 0.0;
            // MATLAB peaks formula (Press et al., adapted from MATLAB source).
            const double xm1   = 1.0 - x;
            const double yp1   = y + 1.0;
            const double term1 = 3.0 * xm1 * xm1
                                  * std::exp(-(x * x) - yp1 * yp1);
            const double term2 = -10.0 * (x / 5.0 - x * x * x - y * y * y * y * y)
                                  * std::exp(-(x * x) - (y * y));
            const double xp1   = x + 1.0;
            const double term3 = -std::exp(-(xp1 * xp1) - (y * y)) / 3.0;
            zd[j * n + i] = term1 + term2 + term3;
        }
    }
    return Z;
}

// ── pagemtimes: page-wise matrix multiply ──────────────────────────────
//
// MATLAB R2020b+ batched matmul. Treats axes 1-2 of each operand as the
// matrix (M×K, K×N) and axes ≥3 as a batch index. Output batch shape is
// the NumPy broadcast of the two batch shapes. Supports DOUBLE and
// SINGLE (mixed → SINGLE, matching MATLAB's promotion rule).
//
//   Z = pagemtimes(X, Y)                     // tx = ty = None
//   Z = pagemtimes(X, "transpose", Y, "none")
//
// 'transpose' / 'ctranspose' transpose each X (or Y) page before
// multiply. For real input the two flags are identical (no imaginary
// component to conjugate). 2D × 2D collapses to ordinary matmul. One
// operand may be 2D (broadcast across the other's batch dims).

namespace {

// Per-page matmul kernel, parameterised by element type. The DOUBLE
// specialisation hands off to the SIMD-aware matmulDoubleLoop in the
// backend; the SINGLE one uses the same (j, k, i) ordering as a
// portable inline loop.
template <typename T>
inline void runPageMatmul(const T *, const T *, T *,
                          size_t, size_t, size_t);

template <>
inline void runPageMatmul<double>(const double *a, const double *b, double *c,
                                  size_t M, size_t N, size_t K)
{
    detail::matmulDoubleLoop(a, b, c, M, N, K);
}

template <>
inline void runPageMatmul<float>(const float *a, const float *b, float *c,
                                 size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        float *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = 0.0f;
        for (size_t k = 0; k < K; ++k) {
            const float bkj = b[j * K + k];
            const float *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <>
inline void runPageMatmul<Complex>(const Complex *a, const Complex *b, Complex *c,
                                   size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        Complex *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = Complex(0.0, 0.0);
        for (size_t k = 0; k < K; ++k) {
            const Complex bkj = b[j * K + k];
            const Complex *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <typename T> constexpr ValueType pagemtimesElemMType();
template <> constexpr ValueType pagemtimesElemMType<double >() { return ValueType::DOUBLE;  }
template <> constexpr ValueType pagemtimesElemMType<float  >() { return ValueType::SINGLE;  }
template <> constexpr ValueType pagemtimesElemMType<Complex>() { return ValueType::COMPLEX; }

// Read element i of `src` as T. For T = Complex, real-typed sources
// upgrade to (real, 0); for T ∈ {double, float}, complex sources are
// rejected upstream so we never reach the if-branch with COMPLEX input.
template <typename T>
inline T readElemAsT(const Value &src, size_t i, bool typeMatches)
{
    if constexpr (std::is_same_v<T, Complex>) {
        if (typeMatches) return src.complexData()[i];
        return Complex(src.elemAsDouble(i), 0.0);
    } else {
        if (typeMatches) return static_cast<const T *>(src.rawData())[i];
        return static_cast<T>(src.elemAsDouble(i));
    }
}

// Conjugate a value if T is Complex; identity for real T.
template <typename T>
inline T conjIfComplex(T v)
{
    if constexpr (std::is_same_v<T, Complex>) return std::conj(v);
    else return v;
}

// Materialise one page from `src` into typed scratch `dst`, optionally
// transposing (and conjugating, for ctranspose on Complex). Direct copy
// (no per-element conversion) when src already holds the target type
// AND no transpose is needed.
template <typename T>
void materialisePage(T *dst, const Value &src, size_t pageOff,
                     size_t rowDim, size_t colDim, TranspOp tr)
{
    const size_t pageElems = rowDim * colDim;
    const size_t base = pageOff * pageElems;
    const bool typeMatches = (src.type() == pagemtimesElemMType<T>());

    if (tr == TranspOp::None) {
        if (typeMatches) {
            std::memcpy(dst, static_cast<const T *>(src.rawData()) + base,
                        pageElems * sizeof(T));
        } else {
            for (size_t i = 0; i < pageElems; ++i)
                dst[i] = readElemAsT<T>(src, base + i, false);
        }
        return;
    }
    // Transpose: dst is colDim × rowDim col-major;
    // dst[r * colDim + c] = src[c * rowDim + r] (then conjugate if ctranspose+Complex).
    const bool needsConj = (tr == TranspOp::CTranspose);
    for (size_t r = 0; r < rowDim; ++r) {
        for (size_t c = 0; c < colDim; ++c) {
            const size_t srcOff = base + c * rowDim + r;
            T v = readElemAsT<T>(src, srcOff, typeMatches);
            if (needsConj) v = conjIfComplex<T>(v);
            dst[r * colDim + c] = v;
        }
    }
}

template <typename T>
Value pagemtimesImpl(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr)
{
    const auto &xd = x.dims();
    const auto &yd = y.dims();
    const int xnd = xd.ndim();
    const int ynd = yd.ndim();
    if (xnd < 2 || ynd < 2)
        throw Error("pagemtimes: each input must have at least 2 dimensions",
                     0, 0, "pagemtimes", "", "numkit:pagemtimes:rank");

    const size_t xRowDim = xd.dim(0), xColDim = xd.dim(1);
    const size_t yRowDim = yd.dim(0), yColDim = yd.dim(1);

    const size_t M  = (tx == TranspOp::None) ? xRowDim : xColDim;
    const size_t Kx = (tx == TranspOp::None) ? xColDim : xRowDim;
    const size_t Ky = (ty == TranspOp::None) ? yRowDim : yColDim;
    const size_t N  = (ty == TranspOp::None) ? yColDim : yRowDim;
    if (Kx != Ky)
        throw Error("pagemtimes: inner matrix dimensions must agree",
                     0, 0, "pagemtimes", "", "numkit:pagemtimes:innerdim");
    const size_t K = Kx;

    constexpr int kMaxNd = Dims::kMaxRank;
    const int xb = std::max(0, xnd - 2);
    const int yb = std::max(0, ynd - 2);
    const int outBatchNd = std::max(xb, yb);
    size_t xBatch[kMaxNd], yBatch[kMaxNd], outBatch[kMaxNd];
    for (int i = 0; i < outBatchNd; ++i) {
        xBatch[i] = (i < xb) ? xd.dim(2 + i) : 1;
        yBatch[i] = (i < yb) ? yd.dim(2 + i) : 1;
        if (xBatch[i] != yBatch[i] && xBatch[i] != 1 && yBatch[i] != 1)
            throw Error("pagemtimes: batch dimensions must broadcast "
                         "(each axis must match or be 1)",
                         0, 0, "pagemtimes", "", "numkit:pagemtimes:dimagree");
        outBatch[i] = std::max(xBatch[i], yBatch[i]);
    }

    size_t batchN = 1;
    for (int i = 0; i < outBatchNd; ++i) batchN *= outBatch[i];

    const int outNd = 2 + outBatchNd;
    size_t outDimArr[kMaxNd];
    outDimArr[0] = M;
    outDimArr[1] = N;
    for (int i = 0; i < outBatchNd; ++i) outDimArr[2 + i] = outBatch[i];
    auto z = createForDims(Dims(outDimArr, outNd), pagemtimesElemMType<T>(), mr);
    if (M == 0 || N == 0 || batchN == 0)
        return z;

    T *zData = static_cast<T *>(z.rawDataMut());
    const size_t xPageStride = xRowDim * xColDim;
    const size_t yPageStride = yRowDim * yColDim;
    const size_t zPageStride = M * N;

    // Direct-pass when source already matches T and no transpose is
    // needed; otherwise materialise into typed scratch (one per call,
    // reused across all batch pages).
    const bool xDirect = (x.type() == pagemtimesElemMType<T>()) && (tx == TranspOp::None);
    const bool yDirect = (y.type() == pagemtimesElemMType<T>()) && (ty == TranspOp::None);
    ScratchArena scratch(mr);
    ScratchVec<T> scratchX(&scratch), scratchY(&scratch);
    if (!xDirect) scratchX.resize(xPageStride);
    if (!yDirect) scratchY.resize(yPageStride);

    auto getXPage = [&](size_t pageOff) -> const T * {
        if (xDirect)
            return static_cast<const T *>(x.rawData()) + pageOff * xPageStride;
        materialisePage(scratchX.data(), x, pageOff, xRowDim, xColDim, tx);
        return scratchX.data();
    };
    auto getYPage = [&](size_t pageOff) -> const T * {
        if (yDirect)
            return static_cast<const T *>(y.rawData()) + pageOff * yPageStride;
        materialisePage(scratchY.data(), y, pageOff, yRowDim, yColDim, ty);
        return scratchY.data();
    };

    if (outBatchNd == 0) {
        runPageMatmul<T>(getXPage(0), getYPage(0), zData, M, N, K);
        return z;
    }

    size_t xBatchStride[kMaxNd], yBatchStride[kMaxNd];
    {
        size_t sx = 1, sy = 1;
        for (int i = 0; i < outBatchNd; ++i) {
            xBatchStride[i] = sx;
            yBatchStride[i] = sy;
            sx *= xBatch[i];
            sy *= yBatch[i];
        }
    }

    size_t coords[kMaxNd] = {0};
    Dims outBatchDims(outBatch, outBatchNd);
    size_t pageIdx = 0;
    do {
        size_t xOff = 0, yOff = 0;
        for (int i = 0; i < outBatchNd; ++i) {
            const size_t xc = (xBatch[i] == 1) ? 0 : coords[i];
            const size_t yc = (yBatch[i] == 1) ? 0 : coords[i];
            xOff += xc * xBatchStride[i];
            yOff += yc * yBatchStride[i];
        }
        runPageMatmul<T>(getXPage(xOff), getYPage(yOff),
                         zData + pageIdx * zPageStride,
                         M, N, K);
        ++pageIdx;
    } while (incrementCoords(coords, outBatchDims));

    return z;
}

} // namespace

Value pagemtimes(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    return pagemtimes(x, TranspOp::None, y, TranspOp::None, mr);
}

Value pagemtimes(const Value &x, TranspOp tx, const Value &y, TranspOp ty, std::pmr::memory_resource *mr)
{
    // MATLAB type promotion: COMPLEX wins over real; SINGLE wins over
    // DOUBLE. Integer/logical/char inputs are rejected — pagemtimes
    // requires floating or complex inputs.
    auto isFloatLike = [](ValueType t) {
        return t == ValueType::DOUBLE || t == ValueType::SINGLE || t == ValueType::COMPLEX;
    };
    if (!isFloatLike(x.type()) || !isFloatLike(y.type()))
        throw Error("pagemtimes: inputs must be 'single', 'double', or complex",
                     0, 0, "pagemtimes", "", "numkit:pagemtimes:type");
    if (x.isComplex() || y.isComplex())
        return pagemtimesImpl<Complex>(x, tx, y, ty, mr);
    if (x.type() == ValueType::SINGLE || y.type() == ValueType::SINGLE)
        return pagemtimesImpl<float  >(x, tx, y, ty, mr);
    return     pagemtimesImpl<double >(x, tx, y, ty, mr);
}

Value diag(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.dims().isVector()) {
        const size_t n = x.numel();
        auto r = Value::matrix(n, n, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < n; ++i)
            r.elem(i, i) = x.doubleData()[i];
        return r;
    }
    const size_t n = std::min(x.dims().rows(), x.dims().cols());
    auto r = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < n; ++i)
        r.doubleDataMut()[i] = x(i, i);
    return r;
}

// ── Sort / find ──────────────────────────────────────────────────────
// Complex sort (MATLAB rule): order by magnitude |z| ascending; ties broken
// by phase angle arg(z) in (-pi, pi] ascending. A NaN component (|z| = NaN)
// sorts LAST for ascending, FIRST for descending. 'descend' fully reverses
// both keys. Unlike min/max there is NO all-real fast path — a COMPLEX-typed
// all-real input still sorts by |z|+angle (so sort([2 -2]+0i) = [2 -2]).
std::tuple<Value, Value> sortComplex(const Value &x, int dim, bool descend,
                                     std::pmr::memory_resource *mr)
{
    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t P = x.dims().is3D() ? x.dims().pages() : 1;
    const int sortDim = (dim >= 1) ? std::min(dim - 1, 2)
                                   : ((R > 1) ? 0 : (C > 1) ? 1 : 2);
    const size_t N = (sortDim == 0) ? R : (sortDim == 1) ? C : P;

    auto r = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::COMPLEX, mr)
                             : Value::matrix(R, C, ValueType::COMPLEX, mr);
    auto idx = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                               : Value::matrix(R, C, ValueType::DOUBLE, mr);

    const size_t slice0 = (sortDim == 0) ? 1 : R;
    const size_t slice1 = (sortDim == 1) ? 1 : C;
    const size_t slice2 = (sortDim == 2) ? 1 : P;
    const Complex *src = x.complexData();
    ScratchArena scratch(mr);
    ScratchVec<std::pair<Complex, size_t>> buf(N, &scratch);

    for (size_t pp = 0; pp < slice2; ++pp)
        for (size_t c = 0; c < slice1; ++c)
            for (size_t rr = 0; rr < slice0; ++rr) {
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    buf[k] = {src[pIdx * R * C + cIdx * R + rIdx], k};
                }
                std::stable_sort(buf.begin(), buf.end(),
                          [descend](const auto &a, const auto &b) {
                              const double am = std::abs(a.first), bm = std::abs(b.first);
                              const bool an = std::isnan(am), bn = std::isnan(bm);
                              if (an || bn) {
                                  if (an && bn) return false;
                                  return descend ? an : bn;
                              }
                              if (am != bm) return descend ? (am > bm) : (am < bm);
                              const double aa = std::arg(a.first), ba = std::arg(b.first);
                              if (aa != ba) return descend ? (aa > ba) : (aa < ba);
                              return false;
                          });
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    const size_t lin = pIdx * R * C + cIdx * R + rIdx;
                    r.complexDataMut()[lin] = buf[k].first;
                    idx.doubleDataMut()[lin] = static_cast<double>(buf[k].second + 1);
                }
            }
    return std::make_tuple(std::move(r), std::move(idx));
}

std::tuple<Value, Value> sort(const Value &x, int dim, bool descend,
                              std::pmr::memory_resource *mr)
{
    if (x.isScalar())
        return std::make_tuple(x, Value::scalar(1.0, mr));
    if (x.type() == ValueType::COMPLEX)
        return sortComplex(x, dim, descend, mr);

    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t P = x.dims().is3D() ? x.dims().pages() : 1;
    // dim<1 -> auto (first non-singleton, MATLAB convention). Explicit dim
    // is 1-based: 1=down columns, 2=along rows, 3=along pages.
    const int sortDim = (dim >= 1) ? std::min(dim - 1, 2)
                                   : ((R > 1) ? 0 : (C > 1) ? 1 : 2);
    const size_t N = (sortDim == 0) ? R : (sortDim == 1) ? C : P;

    auto r = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                             : Value::matrix(R, C, ValueType::DOUBLE, mr);
    auto idx = x.dims().is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                               : Value::matrix(R, C, ValueType::DOUBLE, mr);

    const size_t slice0 = (sortDim == 0) ? 1 : R;
    const size_t slice1 = (sortDim == 1) ? 1 : C;
    const size_t slice2 = (sortDim == 2) ? 1 : P;
    ScratchArena scratch(mr);
    ScratchVec<std::pair<double, size_t>> buf(N, &scratch);

    for (size_t pp = 0; pp < slice2; ++pp)
        for (size_t c = 0; c < slice1; ++c)
            for (size_t rr = 0; rr < slice0; ++rr) {
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    buf[k] = {x.doubleData()[pIdx * R * C + cIdx * R + rIdx], k};
                }
                // MATLAB sort: stable; NaN sorts LAST for ascend, FIRST
                // for descend. std::stable_sort keeps the original index
                // order for ties (matches MATLAB's [s,i]).
                std::stable_sort(buf.begin(), buf.end(),
                          [descend](const auto &a, const auto &b) {
                              const double av = a.first, bv = b.first;
                              const bool an = std::isnan(av), bn = std::isnan(bv);
                              if (an || bn) {
                                  if (an && bn) return false;
                                  return descend ? an : bn;
                              }
                              return descend ? (av > bv) : (av < bv);
                          });
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    const size_t lin = pIdx * R * C + cIdx * R + rIdx;
                    r.doubleDataMut()[lin] = buf[k].first;
                    idx.doubleDataMut()[lin] = static_cast<double>(buf[k].second + 1);
                }
            }
    return std::make_tuple(std::move(r), std::move(idx));
}

// ── sortrows ─────────────────────────────────────────────────────────
namespace {

// Promote to a 2D DOUBLE matrix for row-tuple ops. Returns a copy if the
// type or shape differs; for already-2D-DOUBLE input returns by value
// (cheap COW in the engine).
Value toDoubleMatrix2D(const Value &x, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error(std::string(fn) + ": input must be 2D",
                     0, 0, fn, "", std::string("numkit:") + fn + ":bad2D");
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    if (x.type() == ValueType::DOUBLE) {
        // Return a fresh DOUBLE matrix identical to x — cheap, avoids
        // touching the input through a shared buffer later.
        auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
        if (x.numel() > 0)
            std::memcpy(r.doubleDataMut(), x.doubleData(),
                        x.numel() * sizeof(double));
        return r;
    }
    auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < x.numel(); ++i)
        dst[i] = x.elemAsDouble(i);
    return r;
}

// Complex sortrows (MATLAB rule): rows are ordered lexicographically by the
// requested columns; each column compares complex entries by magnitude |z|
// then phase angle arg(z) ascending (a negative column index sorts that
// column descending). A NaN component sorts last. Must NOT drop the
// imaginary part (toDoubleMatrix2D does) — that silently returns wrong rows.
inline bool cxRowLess(Complex a, Complex b)
{
    const double am = std::abs(a), bm = std::abs(b);
    const bool an = std::isnan(am), bn = std::isnan(bm);
    if (an || bn) { if (an && bn) return false; return bn; }   // non-NaN < NaN
    if (am != bm) return am < bm;
    return std::arg(a) < std::arg(b);
}

std::tuple<Value, Value>
sortRowsComplex(const Value &x, const int *cols, std::size_t nCols,
                std::pmr::memory_resource *mr)
{
    if (x.dims().is3D() || x.dims().ndim() > 2)
        throw Error("sortrows: input must be 2D",
                     0, 0, "sortrows", "", "numkit:sortrows:bad2D");
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    if (R == 0)
        return std::make_tuple(x, Value::matrix(0, 1, ValueType::DOUBLE, mr));

    ScratchArena scratch(mr);
    ScratchVec<int> sortKeys(&scratch);
    if (nCols == 0) {
        sortKeys.reserve(C);
        for (size_t c = 1; c <= C; ++c) sortKeys.push_back(static_cast<int>(c));
    } else {
        sortKeys.assign(cols, cols + nCols);
        for (int rawCol : sortKeys) {
            const int absC = (rawCol < 0) ? -rawCol : rawCol;
            if (rawCol == 0 || static_cast<size_t>(absC) > C)
                throw Error("sortrows: column index out of range",
                             0, 0, "sortrows", "", "numkit:sortrows:badCol");
        }
    }

    const Complex *src = x.complexData();
    auto perm = ScratchVec<size_t>(R, &scratch);
    for (size_t i = 0; i < R; ++i) perm[i] = i;
    std::stable_sort(perm.begin(), perm.end(),
        [&](size_t a, size_t b) {
            for (int key : sortKeys) {
                const bool desc = key < 0;
                const size_t c = static_cast<size_t>((desc ? -key : key) - 1);
                const Complex va = src[c * R + a], vb = src[c * R + b];
                if (cxRowLess(va, vb)) return !desc;
                if (cxRowLess(vb, va)) return desc;
            }
            return false;   // all keys equal → stable
        });

    auto out = Value::matrix(R, C, ValueType::COMPLEX, mr);
    Complex *dst = out.complexDataMut();
    for (size_t i = 0; i < R; ++i)
        for (size_t c = 0; c < C; ++c)
            dst[c * R + i] = src[c * R + perm[i]];
    auto idx = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    double *idxP = idx.doubleDataMut();
    for (size_t i = 0; i < R; ++i)
        idxP[i] = static_cast<double>(perm[i] + 1);
    return std::make_tuple(std::move(out), std::move(idx));
}

std::tuple<Value, Value>
sortRowsImpl(const Value &x, const int *cols, std::size_t nCols, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX)
        return sortRowsComplex(x, cols, nCols, mr);
    auto m = toDoubleMatrix2D(x, "sortrows", mr);
    const size_t R = m.dims().rows();
    const size_t C = m.dims().cols();

    if (R == 0) {
        // Empty rows — return as-is and an empty 0×1 idx column.
        auto idx = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return std::make_tuple(std::move(m), std::move(idx));
    }

    ScratchArena scratch(mr);

    // Validate cols list. nCols==0 ⇒ all columns ascending in order.
    ScratchVec<int> sortKeys(&scratch);
    if (nCols == 0) {
        sortKeys.reserve(C);
        for (size_t c = 1; c <= C; ++c)
            sortKeys.push_back(static_cast<int>(c));
    } else {
        sortKeys.assign(cols, cols + nCols);
        for (int rawCol : sortKeys) {
            const int absC = (rawCol < 0) ? -rawCol : rawCol;
            if (rawCol == 0 || static_cast<size_t>(absC) > C)
                throw Error("sortrows: column index out of range",
                             0, 0, "sortrows", "", "numkit:sortrows:badCol");
        }
    }

    auto perm = ScratchVec<size_t>(R, &scratch);
    for (size_t i = 0; i < R; ++i) perm[i] = i;

    const double *src = m.doubleData();
    std::stable_sort(perm.begin(), perm.end(),
        [&](size_t a, size_t b) {
            return detail::rowLexCmpByCols(src, C, R, a, b,
                                            sortKeys.data(), sortKeys.size()) < 0;
        });

    auto sorted = detail::collectRowsByIndex(mr, m, perm.data(), perm.size());
    auto idx = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    double *idxP = idx.doubleDataMut();
    for (size_t i = 0; i < R; ++i)
        idxP[i] = static_cast<double>(perm[i] + 1);
    return std::make_tuple(std::move(sorted), std::move(idx));
}

} // namespace

std::tuple<Value, Value> sortrows(const Value &x, std::pmr::memory_resource *mr)
{
    return sortRowsImpl(x, nullptr, 0, mr);
}

std::tuple<Value, Value> sortrows(const Value &x, Span<const int> cols, std::pmr::memory_resource *mr)
{
    return sortRowsImpl(x, cols.data(), cols.size(), mr);
}

Value find(const Value &x, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto indices = ScratchVec<double>(&scratch);
    if (x.isLogical()) {
        const uint8_t *ld = x.logicalData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (ld[i])
                indices.push_back(static_cast<double>(i + 1));
    } else {
        const double *dd = x.doubleData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (dd[i] != 0.0)
                indices.push_back(static_cast<double>(i + 1));
    }
    const bool rowResult = !x.dims().is3D() && x.dims().rows() == 1;
    auto r = rowResult ? Value::matrix(1, indices.size(), ValueType::DOUBLE, mr)
                       : Value::matrix(indices.size(), 1, ValueType::DOUBLE, mr);
    if (!indices.empty())
        std::memcpy(r.doubleDataMut(), indices.data(), indices.size() * sizeof(double));
    return r;
}

// ── nnz / nonzeros ───────────────────────────────────────────────────
namespace {

// Type-aware predicate: element at linear index i non-zero?
// NaN counts as non-zero (NaN != 0). For COMPLEX both parts checked.
template <typename T>
inline bool isNonzeroElemT(const T *p, size_t i) { return p[i] != T{0}; }

inline bool isNonzeroComplex(const Complex *p, size_t i)
{
    return p[i].real() != 0.0 || p[i].imag() != 0.0;
}

template <typename Visit>
void forEachNonzero(const Value &x, Visit visit)
{
    const size_t n = x.numel();
    switch (x.type()) {
    case ValueType::LOGICAL: {
        const uint8_t *p = x.logicalData();
        for (size_t i = 0; i < n; ++i) if (p[i]) visit(i);
        break;
    }
    case ValueType::DOUBLE: {
        const double *p = x.doubleData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::SINGLE: {
        const float *p = x.singleData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::COMPLEX: {
        const Complex *p = x.complexData();
        for (size_t i = 0; i < n; ++i) if (isNonzeroComplex(p, i)) visit(i);
        break;
    }
    case ValueType::INT8: {
        const int8_t *p = x.int8Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT16: {
        const int16_t *p = x.int16Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT32: {
        const int32_t *p = x.int32Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::INT64: {
        const int64_t *p = x.int64Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT8: {
        const uint8_t *p = x.uint8Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT16: {
        const uint16_t *p = x.uint16Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT32: {
        const uint32_t *p = x.uint32Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    case ValueType::UINT64: {
        const uint64_t *p = x.uint64Data();
        for (size_t i = 0; i < n; ++i) if (isNonzeroElemT(p, i)) visit(i);
        break;
    }
    default:
        throw Error("nnz/nonzeros: unsupported element type",
                     0, 0, "nnz", "", "numkit:nnz:badType");
    }
}

template <typename T>
T *typedDstFor(Value &r, ValueType outType)
{
    switch (outType) {
    case ValueType::LOGICAL: return reinterpret_cast<T *>(r.logicalDataMut());
    case ValueType::DOUBLE:  return reinterpret_cast<T *>(r.doubleDataMut());
    case ValueType::SINGLE:  return reinterpret_cast<T *>(r.singleDataMut());
    case ValueType::COMPLEX: return reinterpret_cast<T *>(r.complexDataMut());
    case ValueType::INT8:    return reinterpret_cast<T *>(r.int8DataMut());
    case ValueType::INT16:   return reinterpret_cast<T *>(r.int16DataMut());
    case ValueType::INT32:   return reinterpret_cast<T *>(r.int32DataMut());
    case ValueType::INT64:   return reinterpret_cast<T *>(r.int64DataMut());
    case ValueType::UINT8:   return reinterpret_cast<T *>(r.uint8DataMut());
    case ValueType::UINT16:  return reinterpret_cast<T *>(r.uint16DataMut());
    case ValueType::UINT32:  return reinterpret_cast<T *>(r.uint32DataMut());
    case ValueType::UINT64:  return reinterpret_cast<T *>(r.uint64DataMut());
    default: return nullptr;
    }
}

template <typename T, typename Reader>
Value collectTypedNonzeros(const Value &x, ValueType outType, Reader read, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<T> vals(&scratch);
    forEachNonzero(x, [&](size_t i) { vals.push_back(read(i)); });
    auto r = Value::matrix(vals.size(), 1, outType, mr);
    if (!vals.empty()) {
        T *dst = typedDstFor<T>(r, outType);
        std::memcpy(dst, vals.data(), vals.size() * sizeof(T));
    }
    return r;
}

} // namespace

Value nnz(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0)
        return Value::scalar(0.0, mr);
    size_t count = 0;
    forEachNonzero(x, [&](size_t) { ++count; });
    return Value::scalar(static_cast<double>(count), mr);
}

Value nonzeros(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) {
        // Empty input → 0×1 column of the source type (DOUBLE if unknown).
        const ValueType outT = (x.type() == ValueType::EMPTY) ? ValueType::DOUBLE : x.type();
        return Value::matrix(0, 1, outT, mr);
    }
    switch (x.type()) {
    case ValueType::LOGICAL: {
        const uint8_t *p = x.logicalData();
        return collectTypedNonzeros<uint8_t>(x, ValueType::LOGICAL, [&](size_t i) -> uint8_t { return p[i]; }, mr);
    }
    case ValueType::DOUBLE: {
        const double *p = x.doubleData();
        return collectTypedNonzeros<double>(x, ValueType::DOUBLE, [&](size_t i) -> double { return p[i]; }, mr);
    }
    case ValueType::SINGLE: {
        const float *p = x.singleData();
        return collectTypedNonzeros<float>(x, ValueType::SINGLE, [&](size_t i) -> float { return p[i]; }, mr);
    }
    case ValueType::COMPLEX: {
        const Complex *p = x.complexData();
        return collectTypedNonzeros<Complex>(x, ValueType::COMPLEX, [&](size_t i) -> Complex { return p[i]; }, mr);
    }
    case ValueType::INT8: {
        const int8_t *p = x.int8Data();
        return collectTypedNonzeros<int8_t>(x, ValueType::INT8, [&](size_t i) -> int8_t { return p[i]; }, mr);
    }
    case ValueType::INT16: {
        const int16_t *p = x.int16Data();
        return collectTypedNonzeros<int16_t>(x, ValueType::INT16, [&](size_t i) -> int16_t { return p[i]; }, mr);
    }
    case ValueType::INT32: {
        const int32_t *p = x.int32Data();
        return collectTypedNonzeros<int32_t>(x, ValueType::INT32, [&](size_t i) -> int32_t { return p[i]; }, mr);
    }
    case ValueType::INT64: {
        const int64_t *p = x.int64Data();
        return collectTypedNonzeros<int64_t>(x, ValueType::INT64, [&](size_t i) -> int64_t { return p[i]; }, mr);
    }
    case ValueType::UINT8: {
        const uint8_t *p = x.uint8Data();
        return collectTypedNonzeros<uint8_t>(x, ValueType::UINT8, [&](size_t i) -> uint8_t { return p[i]; }, mr);
    }
    case ValueType::UINT16: {
        const uint16_t *p = x.uint16Data();
        return collectTypedNonzeros<uint16_t>(x, ValueType::UINT16, [&](size_t i) -> uint16_t { return p[i]; }, mr);
    }
    case ValueType::UINT32: {
        const uint32_t *p = x.uint32Data();
        return collectTypedNonzeros<uint32_t>(x, ValueType::UINT32, [&](size_t i) -> uint32_t { return p[i]; }, mr);
    }
    case ValueType::UINT64: {
        const uint64_t *p = x.uint64Data();
        return collectTypedNonzeros<uint64_t>(x, ValueType::UINT64, [&](size_t i) -> uint64_t { return p[i]; }, mr);
    }
    default:
        throw Error("nonzeros: unsupported element type",
                     0, 0, "nonzeros", "", "numkit:nonzeros:badType");
    }
}

// ── Concatenation ────────────────────────────────────────────────────
Value horzcat(Span<const Value> values, std::pmr::memory_resource *mr)
{
    if (values.empty())
        return Value::empty();
    return Value::horzcat(values.data(), values.size(), mr);
}

Value vertcat(Span<const Value> values, std::pmr::memory_resource *mr)
{
    if (values.empty())
        return Value::empty();
    return Value::vertcat(values.data(), values.size(), mr);
}

// ── Grids ────────────────────────────────────────────────────────────
std::tuple<Value, Value> meshgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel();
    auto X = Value::matrix(ny, nx, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(ny, nx, ValueType::DOUBLE, mr);
    for (size_t r = 0; r < ny; ++r)
        for (size_t c = 0; c < nx; ++c) {
            X.elem(r, c) = x.doubleData()[c];
            Y.elem(r, c) = y.doubleData()[r];
        }
    return std::make_tuple(std::move(X), std::move(Y));
}

// 3-arg meshgrid: returns three [ny, nx, nz] 3-D arrays. See BUGS.md #23.
std::tuple<Value, Value, Value>
meshgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel(), nz = z.numel();
    auto X = Value::matrix3d(ny, nx, nz, ValueType::DOUBLE, mr);
    auto Y = Value::matrix3d(ny, nx, nz, ValueType::DOUBLE, mr);
    auto Z = Value::matrix3d(ny, nx, nz, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    double *yd = Y.doubleDataMut();
    double *zd = Z.doubleDataMut();
    for (size_t p = 0; p < nz; ++p) {
        const double zp = z.elemAsDouble(p);
        for (size_t c = 0; c < nx; ++c) {
            const double xc = x.elemAsDouble(c);
            for (size_t r = 0; r < ny; ++r) {
                const size_t idx = r + c * ny + p * (nx * ny);
                xd[idx] = xc;
                yd[idx] = y.elemAsDouble(r);
                zd[idx] = zp;
            }
        }
    }
    return std::make_tuple(std::move(X), std::move(Y), std::move(Z));
}

// ── ndgrid ──────────────────────────────────────────────────────────
std::tuple<Value, Value>
ndgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel();
    // Output shape: [nx, ny] — first arg is row dim (axes-major).
    auto X = Value::matrix(nx, ny, ValueType::DOUBLE, mr);
    auto Y = Value::matrix(nx, ny, ValueType::DOUBLE, mr);
    for (size_t r = 0; r < nx; ++r)
        for (size_t c = 0; c < ny; ++c) {
            X.elem(r, c) = x.elemAsDouble(r);
            Y.elem(r, c) = y.elemAsDouble(c);
        }
    return std::make_tuple(std::move(X), std::move(Y));
}

std::tuple<Value, Value, Value>
ndgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    const size_t nx = x.numel(), ny = y.numel(), nz = z.numel();
    auto X = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    auto Y = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    auto Z = Value::matrix3d(nx, ny, nz, ValueType::DOUBLE, mr);
    for (size_t p = 0; p < nz; ++p)
        for (size_t c = 0; c < ny; ++c)
            for (size_t r = 0; r < nx; ++r) {
                X.elem(r, c, p) = x.elemAsDouble(r);
                Y.elem(r, c, p) = y.elemAsDouble(c);
                Z.elem(r, c, p) = z.elemAsDouble(p);
            }
    return std::make_tuple(std::move(X), std::move(Y), std::move(Z));
}

// ── kron ────────────────────────────────────────────────────────────
// NOTE: kron / cross / dot migrated to libs/linalg/src/vector_ops.cpp.

// ── Reductions and products ──────────────────────────────────────────

namespace {

// Integer cumsum / cumprod. MATLAB keeps the integer class and accumulates
// NATIVELY with saturation at each step — the saturated running value is
// carried forward, so cumsum(int8([100 100 -100]))=[100 127 27] int8 and
// cumprod(int8([5 10 10]))=[5 50 127] int8. Generic strided scan over the
// chosen dimension d (column-major: stride = prod(dims[0..d-2]),
// len = dims[d-1]). Linear iteration is valid for any dim because element i
// depends only on i-stride (< i). NOTE: int64/uint64 above 2^53 lose
// precision (accumulated through double) — the same limitation as the rest
// of numkit's numeric core; int8/16/32 and uint8/16/32 are exact.
template <typename T>
void cumIntegerScanInto(const Value &x, T *dst, size_t strideD, size_t lenD,
                        bool isProd)
{
    const double lo = static_cast<double>(std::numeric_limits<T>::min());
    const double hi = static_cast<double>(std::numeric_limits<T>::max());
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i) {
        const size_t coord = (i / strideD) % lenD;
        const double cur = x.elemAsDouble(i);
        double v = cur;
        if (coord != 0) {
            const double prev = static_cast<double>(dst[i - strideD]);
            v = isProd ? prev * cur : prev + cur;
        }
        if (v < lo) v = lo;
        else if (v > hi) v = hi;        // saturate to the class range
        dst[i] = static_cast<T>(v);
    }
}

Value cumIntegerNative(const Value &x, int dim, bool isProd,
                       std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();
    int d;
    if (dim > 0) {
        d = detail::resolveDim(x, dim, isProd ? "cumprod" : "cumsum");
    } else {
        d = 1;                          // first non-singleton dim (MATLAB default)
        for (int k = 0; k < nd; ++k)
            if (dd.dim(k) > 1) { d = k + 1; break; }
    }
    size_t strideD = 1;
    for (int k = 0; k < d - 1; ++k) strideD *= dd.dim(k);
    const size_t lenD = (d - 1 < nd) ? dd.dim(d - 1) : 1;

    size_t outDims[Dims::kMaxRank];
    for (int k = 0; k < nd; ++k) outDims[k] = dd.dim(k);
    Value r = Value::matrixND(outDims, nd, x.type(), mr);
    if (x.numel() == 0 || lenD == 0 || strideD == 0) return r;

    switch (x.type()) {
    case ValueType::INT8:   cumIntegerScanInto<int8_t>  (x, r.int8DataMut(),   strideD, lenD, isProd); break;
    case ValueType::INT16:  cumIntegerScanInto<int16_t> (x, r.int16DataMut(),  strideD, lenD, isProd); break;
    case ValueType::INT32:  cumIntegerScanInto<int32_t> (x, r.int32DataMut(),  strideD, lenD, isProd); break;
    case ValueType::INT64:  cumIntegerScanInto<int64_t> (x, r.int64DataMut(),  strideD, lenD, isProd); break;
    case ValueType::UINT8:  cumIntegerScanInto<uint8_t> (x, r.uint8DataMut(),  strideD, lenD, isProd); break;
    case ValueType::UINT16: cumIntegerScanInto<uint16_t>(x, r.uint16DataMut(), strideD, lenD, isProd); break;
    case ValueType::UINT32: cumIntegerScanInto<uint32_t>(x, r.uint32DataMut(), strideD, lenD, isProd); break;
    case ValueType::UINT64: cumIntegerScanInto<uint64_t>(x, r.uint64DataMut(), strideD, lenD, isProd); break;
    default: break;
    }
    return r;
}

} // namespace

Value cumsum(const Value &x, std::pmr::memory_resource *mr)
{
    if (isIntegerType(x.type()))
        return cumIntegerNative(x, 0, /*isProd=*/false, mr);
    if (x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        cumsumScan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }
    const size_t R = x.dims().rows(), C = x.dims().cols();
    auto r = Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    // Per-column inclusive scan — column data is contiguous.
    for (size_t c = 0; c < C; ++c)
        cumsumScan(src + c * R, dst + c * R, R);
    return r;
}

// cumsum along an explicit dim. Output shape equals input shape (this is
// not a reduction). Vector / scalar input ignores dim and walks linearly.
Value cumsum(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (isIntegerType(x.type()))
        return cumIntegerNative(x, dim, /*isProd=*/false, mr);
    if (dim <= 0) return cumsum(x, mr);
    if (x.dims().isVector() || x.isScalar()) return cumsum(x, mr);

    const int d = detail::resolveDim(x, dim, "cumsum");
    const auto &dd = x.dims();

    // ND fallback for rank ≥ 4: per-slice scan along axis d-1. Inner
    // block size B = prod(dim[0..d-2]); outer count O = prod(dim[d..]).
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw Error("cumsum: rank exceeds 32",
                         0, 0, "cumsum", "", "numkit:cumsum:tooManyDims");
        size_t outDims[kMaxNd];
        for (int i = 0; i < dd.ndim(); ++i) outDims[i] = dd.dim(i);
        auto r = Value::matrixND(outDims, dd.ndim(), ValueType::DOUBLE, mr);
        const size_t sliceLen = dd.dim(d - 1);
        size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        if (B == 1) {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                cumsumScan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc += src[base + k * B];
                        dst[base + k * B] = acc;
                    }
                }
        }
        return r;
    }

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                       : Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // dim=1: scan down each column. Column data is contiguous so
        // route through the SIMD prefix-sum kernel.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                cumsumScan(src + base, dst + base, R);
            }
    } else if (d == 2) {
        // Walk across columns for each (row, page). Stride = R.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                const size_t pageBase = pp * R * C;
                for (size_t c = 0; c < C; ++c) {
                    s += src[pageBase + c * R + rr];
                    dst[pageBase + c * R + rr] = s;
                }
            }
    } else if (d == 3) {
        // Walk through pages for each (row, col). Stride = R*C.
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                for (size_t pp = 0; pp < P; ++pp) {
                    s += src[pp * R * C + c * R + rr];
                    dst[pp * R * C + c * R + rr] = s;
                }
            }
    }
    return r;
}

// ── Generic cumulative kernel for cumprod / cummax / cummin ─────────
//
// Op is a binary functor (double, double) -> double. Init is the value
// the running accumulator starts at; for cumulative ops we instead seed
// with the first element of the slice, but seeding behavior is still
// captured by Op (e.g. cumprod could just use init=1.0 multiplicative).
// We use the seed-with-first style so cummax / cummin work without
// needing to know +/- infinity for the initial value.
namespace {

template <typename Op>
void cumKernel(const Value &x, int d, Op op, double *dst)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const double *src = x.doubleData();

    if (d == 1) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                if (R == 0) continue;
                double acc = src[base];
                dst[base] = acc;
                for (size_t rr = 1; rr < R; ++rr) {
                    acc = op(acc, src[base + rr]);
                    dst[base + rr] = acc;
                }
            }
    } else if (d == 2) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                const size_t pageBase = pp * R * C;
                if (C == 0) continue;
                double acc = src[pageBase + rr];
                dst[pageBase + rr] = acc;
                for (size_t c = 1; c < C; ++c) {
                    acc = op(acc, src[pageBase + c * R + rr]);
                    dst[pageBase + c * R + rr] = acc;
                }
            }
    } else if (d == 3) {
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                if (P == 0) continue;
                double acc = src[c * R + rr];
                dst[c * R + rr] = acc;
                for (size_t pp = 1; pp < P; ++pp) {
                    acc = op(acc, src[pp * R * C + c * R + rr]);
                    dst[pp * R * C + c * R + rr] = acc;
                }
            }
    }
}

template <typename Op>
Value cumImpl(const Value &x, int dim, Op op, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    if (x.dims().isVector() || x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(),
                                ValueType::DOUBLE, mr);
        if (x.numel() == 0) return r;
        double acc = x.doubleData()[0];
        r.doubleDataMut()[0] = acc;
        for (size_t i = 1; i < x.numel(); ++i) {
            acc = op(acc, x.doubleData()[i]);
            r.doubleDataMut()[i] = acc;
        }
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();
    auto r = dd.is3D() ? Value::matrix3d(dd.rows(), dd.cols(), dd.pages(),
                                          ValueType::DOUBLE, mr)
                       : Value::matrix(dd.rows(), dd.cols(),
                                        ValueType::DOUBLE, mr);
    cumKernel(x, d, op, r.doubleDataMut());
    return r;
}

} // namespace

// cumprod / cummax / cummin: SIMD prefix-op kernels in
// backends/MStdCumSum_{simd,portable}.cpp handle vector input and the
// dim=1 (column) path where access is contiguous. For dim=2/3 the
// strided access pattern doesn't benefit from SIMD; cumImpl's scalar
// cumKernel still handles those (with the same Op as before).
namespace {

using ScanFn = void (*)(const double *, double *, std::size_t);

template <typename Op>
Value cumScanDispatch(const Value &x, int dim, ScanFn scan, Op scalarOp, const char *fn, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = Value::matrix(x.dims().rows(), x.dims().cols(), ValueType::DOUBLE, mr);
        scan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();

    // ND fallback (rank ≥ 4): per-slice scan along axis d-1.
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw Error(std::string(fn) + ": rank exceeds 32",
                         0, 0, fn, "", std::string("numkit:") + fn + ":tooManyDims");
        size_t outDims[kMaxNd];
        for (int i = 0; i < dd.ndim(); ++i) outDims[i] = dd.dim(i);
        auto r = Value::matrixND(outDims, dd.ndim(), ValueType::DOUBLE, mr);
        const size_t sliceLen = dd.dim(d - 1);
        size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        if (B == 1) {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                scan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc = scalarOp(acc, src[base + k * B]);
                        dst[base + k * B] = acc;
                    }
                }
        }
        return r;
    }

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? Value::matrix3d(R, C, P, ValueType::DOUBLE, mr)
                       : Value::matrix(R, C, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // Per-column scan — column data is contiguous, route through SIMD.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                scan(src + base, dst + base, R);
            }
    } else {
        // dim=2/3: strided access; reuse the existing scalar cumKernel.
        cumKernel(x, d, scalarOp, dst);
    }
    return r;
}

} // namespace

Value cumprod(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (isIntegerType(x.type()))
        return cumIntegerNative(x, dim, /*isProd=*/true, mr);
    return cumScanDispatch(x, dim, cumprodScan, [](double a, double b) { return a * b; }, "cumprod", mr);
}

Value cummax(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    // NaN propagation: MATLAB cummax skips NaN if 'omitnan' is passed
    // and propagates otherwise. Default = 'omitnan' since R2018a; we
    // skip NaN here, treating them as identity.
    return cumScanDispatch(x, dim, cummaxScan, [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::max(a, b);
                           }, "cummax", mr);
}

Value cummin(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    return cumScanDispatch(x, dim, cumminScan, [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::min(a, b);
                           }, "cummin", mr);
}

// ── diff: discrete difference ────────────────────────────────────────
namespace {

// One pass of forward differences along axis `d` (1-based). Source has
// dim[d-1] = sliceLen; output has dim[d-1] = sliceLen - 1. Column-major
// strides (innerStride = prod(dim[0..d-2])).
void diffOnceDouble(const double *src, double *dst,
                    const Dims &srcDims, int d)
{
    const int nd = srcDims.ndim();
    const size_t sliceLen = srcDims.dim(d - 1);
    if (sliceLen < 2) return;  // out has zero elements

    size_t innerStride = 1;
    for (int i = 0; i < d - 1; ++i) innerStride *= srcDims.dim(i);
    size_t outerCount = 1;
    for (int i = d; i < nd; ++i) outerCount *= srcDims.dim(i);
    const size_t outSliceLen = sliceLen - 1;

    if (innerStride == 1) {
        // Contiguous along the diff axis — simple linear pass per outer block.
        for (size_t o = 0; o < outerCount; ++o) {
            const double *s = src + o * sliceLen;
            double *t = dst + o * outSliceLen;
            for (size_t k = 0; k < outSliceLen; ++k)
                t[k] = s[k + 1] - s[k];
        }
    } else {
        for (size_t o = 0; o < outerCount; ++o)
            for (size_t b = 0; b < innerStride; ++b) {
                const size_t srcBase = o * innerStride * sliceLen + b;
                const size_t dstBase = o * innerStride * outSliceLen + b;
                for (size_t k = 0; k < outSliceLen; ++k)
                    dst[dstBase + k * innerStride] =
                        src[srcBase + (k + 1) * innerStride] -
                        src[srcBase + k * innerStride];
            }
    }
}

Value makeDiffOutput(const Dims &srcDims, int d, size_t step, std::pmr::memory_resource *mr)
{
    const int nd = srcDims.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    if (nd > kMaxNd)
        throw Error("diff: rank exceeds 32",
                     0, 0, "diff", "", "numkit:diff:tooManyDims");
    size_t outDims[kMaxNd];
    for (int i = 0; i < nd; ++i) outDims[i] = srcDims.dim(i);
    outDims[d - 1] = (outDims[d - 1] >= step) ? outDims[d - 1] - step : 0;
    return Value::matrixND(outDims, nd, ValueType::DOUBLE, mr);
}

Value copyToDouble(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &dd = x.dims();
    const int nd = dd.ndim();
    constexpr int kMaxNd = Dims::kMaxRank;
    size_t dims[kMaxNd];
    for (int i = 0; i < nd; ++i) dims[i] = dd.dim(i);
    auto r = Value::matrixND(dims, nd, ValueType::DOUBLE, mr);
    if (x.type() == ValueType::DOUBLE) {
        std::memcpy(r.doubleDataMut(), x.doubleData(),
                    x.numel() * sizeof(double));
    } else {
        double *dst = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            dst[i] = x.elemAsDouble(i);
    }
    return r;
}

} // namespace

Value diff(const Value &x, int n, int dim, std::pmr::memory_resource *mr)
{
    if (n < 0)
        throw Error("diff: order n must be non-negative",
                     0, 0, "diff", "", "numkit:diff:badOrder");

    if (n == 0) {
        // Identity copy preserving DOUBLE shape.
        return copyToDouble(x, mr);
    }

    // Scalar: MATLAB returns 1×0 empty.
    if (x.isScalar())
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    const int d = detail::resolveDim(x, dim, "diff");
    const auto &dd = x.dims();
    const size_t sliceLen = (d >= 1 && d <= dd.ndim()) ? dd.dim(d - 1) : 1;

    // If n collapses or exceeds the dim, return correctly-shaped empty.
    if (sliceLen <= static_cast<size_t>(n))
        return makeDiffOutput(dd, d, sliceLen, mr);

    // Promote integer/logical to DOUBLE first (consistent with cumsum).
    Value cur = copyToDouble(x, mr);

    for (int pass = 0; pass < n; ++pass) {
        const auto &curDims = cur.dims();
        auto out = makeDiffOutput(curDims, d, 1, mr);
        diffOnceDouble(cur.doubleData(), out.doubleDataMut(), curDims, d);
        cur = std::move(out);
    }
    return cur;
}

// ── any / all moved to backends/MStdLogicalReductions_{simd,portable}.cpp
//
// MATLAB's any(X) returns true if ANY element is non-zero (NaN counts
// as true since NaN != 0). all(X) returns true if ALL elements are
// non-zero. Empty: any → false, all → true (vacuously). The SIMD
// backend scans LOGICAL bytes and DOUBLE lanes directly with early
// exit (Phase P1 of project_perf_optimization_plan.md).

namespace {

// Used by xor below — small inputs, no need for a SIMD path.
Value promoteToDouble(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::DOUBLE) return x;
    auto r = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.elemAsDouble(i);
    return r;
}

} // namespace

// ── xor (elementwise logical) ────────────────────────────────────────
Value xorOf(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    auto ad = promoteToDouble(a, mr);
    auto bd = promoteToDouble(b, mr);
    auto d = elementwiseDouble(ad, bd,
        [](double aa, double bb) {
            return ((aa != 0.0) != (bb != 0.0)) ? 1.0 : 0.0;
        }, mr);
    if (d.isScalar()) return Value::logicalScalar(d.toScalar() != 0.0, mr);
    auto r = createLike(d, ValueType::LOGICAL, mr);
    for (size_t i = 0; i < d.numel(); ++i)
        r.logicalDataMut()[i] = (d.doubleData()[i] != 0.0) ? 1 : 0;
    return r;
}

// NOTE: cross / dot migrated to libs/linalg/src/vector_ops.cpp.

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void zeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    // Strip trailing class-name (e.g. 'uint8') or 'like' form before
    // parsing dims. Default type is DOUBLE.
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    // Value::matrix*/matrixND zero-fill the buffer for any type, so
    // createMatrixND with the requested type IS the zeros() output.
    outs[0] = createMatrixND(d.data(), d.size(), t, mr);
}

// Fill `v` with one in its declared type (1 / 1.0 / true).
namespace { inline void fillOnes(Value &v, ValueType t)
{
    const size_t n = v.numel();
    if (n == 0) return;
    switch (t) {
      case ValueType::DOUBLE:  { auto *p = v.doubleDataMut();  std::fill(p, p + n, 1.0); break; }
      case ValueType::SINGLE:  { auto *p = v.singleDataMut();  std::fill(p, p + n, 1.0f); break; }
      case ValueType::LOGICAL: { auto *p = v.logicalDataMut(); std::fill(p, p + n, uint8_t(1)); break; }
      case ValueType::INT8:    { auto *p = v.int8DataMut();    std::fill(p, p + n, int8_t(1)); break; }
      case ValueType::INT16:   { auto *p = v.int16DataMut();   std::fill(p, p + n, int16_t(1)); break; }
      case ValueType::INT32:   { auto *p = v.int32DataMut();   std::fill(p, p + n, int32_t(1)); break; }
      case ValueType::INT64:   { auto *p = v.int64DataMut();   std::fill(p, p + n, int64_t(1)); break; }
      case ValueType::UINT8:   { auto *p = v.uint8DataMut();   std::fill(p, p + n, uint8_t(1)); break; }
      case ValueType::UINT16:  { auto *p = v.uint16DataMut();  std::fill(p, p + n, uint16_t(1)); break; }
      case ValueType::UINT32:  { auto *p = v.uint32DataMut();  std::fill(p, p + n, uint32_t(1)); break; }
      case ValueType::UINT64:  { auto *p = v.uint64DataMut();  std::fill(p, p + n, uint64_t(1)); break; }
      default: throw Error("ones: unsupported type for fill",
                           0, 0, "ones", "", "numkit:ones:badType");
    }
}}

void ones_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    fillOnes(m, t);
    outs[0] = std::move(m);
}

// MATLAB's colon function: colon(j, k) = j:k, colon(j, i, k) = j:i:k.
// Useful when the operator form is awkward (function-handle slot, etc.)
// and to be a real callable for parity tests. Type preservation matches
// the operator path (see core/src/tree_walker.cpp:colonOutputType and
// core/src/vm.cpp:OpCode::COLON).
namespace { ValueType colonOutType(const Value *ops, size_t n)
{
    ValueType nonDouble = ValueType::DOUBLE;
    bool found = false;
    for (size_t i = 0; i < n; ++i) {
        ValueType t = ops[i].type();
        if (t == ValueType::DOUBLE) continue;
        if (!found) { nonDouble = t; found = true; }
        else if (t != nonDouble)
            throw Error("colon: operands must be all the same type, "
                        "or mixed with real scalar doubles",
                        0, 0, "colon", "", "numkit:colon:typeMix");
    }
    return nonDouble;
}}

void colon_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        ValueType t = colonOutType(args.data(), 2);
        outs[0] = Value::colonRangeTyped(args[0].toScalar(),
                                          args[1].toScalar(), t, mr);
    } else if (args.size() == 3) {
        ValueType t = colonOutType(args.data(), 3);
        outs[0] = Value::colonRangeTyped(args[0].toScalar(),
                                          args[1].toScalar(),
                                          args[2].toScalar(), t, mr);
    } else {
        throw Error("colon: requires 2 or 3 arguments",
                    0, 0, "colon", "", "numkit:colon:nargin");
    }
}

// MATLAB's sparse() with size args allocates an MxN sparse zero matrix.
// Numkit has no sparse storage class -- this stub returns dense zeros.
// Matches issparse=false (we ship that stub; see types.cpp:issparse).
// KNOWN GAP: numkit returns dense; MATLAB returns sparse storage class.
// All numerical operations match (zeros on both sides), only class()
// differs. Documented in PROGRESS for sparse().
void sparse_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // sparse(M, N) -- allocate MxN dense zeros.
        const size_t M = static_cast<size_t>(args[0].toScalar());
        const size_t N = static_cast<size_t>(args[1].toScalar());
        outs[0] = Value::matrix(M, N, ValueType::DOUBLE, mr);
        return;
    }
    if (args.size() == 1) {
        // sparse(A) -- "convert" dense to sparse. We just return A as-is
        // (since we have no sparse storage). For most numerical use this
        // is correct; isparse() still returns false (matches numkit
        // semantics).
        outs[0] = args[0];
        return;
    }
    throw Error("sparse: numkit has no sparse storage; supports only "
                "sparse(M, N) → dense zeros and sparse(A) → A passthrough",
                0, 0, "sparse", "", "numkit:sparse:NoSparse");
}

// `nan` / `NaN` / `inf` / `Inf` are MATLAB built-in functions (not
// constants): bare `nan` returns scalar NaN; `nan(M, N, ..., 'type')`
// returns float array filled with NaN (only 'double' or 'single' are
// allowed -- integer types can't represent NaN/Inf, MATLAB throws).
// Same shape parsing as zeros/ones.
namespace { void nanInfFill(Value &v, double fillD, float fillS, ValueType t)
{
    const size_t n = v.numel();
    if (n == 0) return;
    if (t == ValueType::DOUBLE) {
        auto *p = v.doubleDataMut(); std::fill(p, p + n, fillD);
    } else if (t == ValueType::SINGLE) {
        auto *p = v.singleDataMut(); std::fill(p, p + n, fillS);
    }
}}

void nan_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
        return;
    }
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("nan: type must be 'double' or 'single' (NaN is float-only)",
                    0, 0, "nan", "", "numkit:nan:badType");
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    nanInfFill(m, std::numeric_limits<double>::quiet_NaN(),
                  std::numeric_limits<float>::quiet_NaN(), t);
    outs[0] = std::move(m);
}

void inf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::scalar(std::numeric_limits<double>::infinity(), mr);
        return;
    }
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("inf: type must be 'double' or 'single' (Inf is float-only)",
                    0, 0, "inf", "", "numkit:inf:badType");
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(d);
    auto m = createMatrixND(d.data(), d.size(), t, mr);
    nanInfFill(m, std::numeric_limits<double>::infinity(),
                  std::numeric_limits<float>::infinity(), t);
    outs[0] = std::move(m);
}

// `true` and `false` are MATLAB built-in functions (not constants):
// bare `true` returns a scalar logical 1; `true(M, N, ...)` returns a
// logical array filled with 1 (or 0 for false). Mirrors zeros/ones
// shape parsing. See BUGS.md #30.
void true_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::logicalScalar(true, mr);
        return;
    }
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    auto v = createMatrixND(d.data(), d.size(), ValueType::LOGICAL, mr);
    uint8_t *p = v.logicalDataMut();
    for (size_t i = 0; i < v.numel(); ++i) p[i] = 1;
    outs[0] = std::move(v);
}

void false_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::logicalScalar(false, mr);
        return;
    }
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    stripTrailingOnes(d);
    // createMatrixND zero-fills LOGICAL by default.
    outs[0] = createMatrixND(d.data(), d.size(), ValueType::LOGICAL, mr);
}

void eye_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    auto d = parseDimsArgs(dimArgs);
    if (t == ValueType::DOUBLE) {
        // Fast path: direct double eye().
        outs[0] = eye(d.rows, d.cols, mr);
        return;
    }
    // Typed eye: zero-fill matrix of `t`, then set diagonal to one.
    auto m = Value::matrix(d.rows, d.cols, t, mr);
    const size_t k = std::min(d.rows, d.cols);
    switch (t) {
      case ValueType::SINGLE: { auto *p = m.singleDataMut(); for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1.0f; break; }
      case ValueType::LOGICAL:{ auto *p = m.logicalDataMut(); for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT8:   { auto *p = m.int8DataMut();    for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT16:  { auto *p = m.int16DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT32:  { auto *p = m.int32DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::INT64:  { auto *p = m.int64DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT8:  { auto *p = m.uint8DataMut();   for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT16: { auto *p = m.uint16DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT32: { auto *p = m.uint32DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      case ValueType::UINT64: { auto *p = m.uint64DataMut();  for (size_t i = 0; i < k; ++i) p[i + i*d.rows] = 1; break; }
      default: throw Error("eye: unsupported type", 0, 0, "eye", "", "numkit:eye:badType");
    }
    outs[0] = std::move(m);
}

void magic_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("magic: requires exactly 1 argument",
                     0, 0, "magic", "", "numkit:magic:nargin");
    const double nd = args[0].toScalar();
    if (nd < 0.0 || nd != std::floor(nd))
        throw Error("magic: N must be a non-negative integer",
                     0, 0, "magic", "", "numkit:magic:badN");
    outs[0] = magic(static_cast<size_t>(nd), ctx.engine->resource());
}

void toeplitz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("toeplitz: requires 1 or 2 arguments",
                    0, 0, "toeplitz", "", "numkit:toeplitz:nargin");
    const Value &r = args.size() == 2 ? args[1] : Value::Empty;
    outs[0] = toeplitz(args[0], r, ctx.engine->resource());
}

void hankel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("hankel: requires 1 or 2 arguments",
                    0, 0, "hankel", "", "numkit:hankel:nargin");
    const Value &r = args.size() == 2 ? args[1] : Value::Empty;
    outs[0] = hankel(args[0], r, ctx.engine->resource());
}

void vander_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("vander: requires exactly 1 argument",
                    0, 0, "vander", "", "numkit:vander:nargin");
    outs[0] = vander(args[0], ctx.engine->resource());
}

void compan_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("compan: requires exactly 1 argument",
                    0, 0, "compan", "", "numkit:compan:nargin");
    outs[0] = compan(args[0], ctx.engine->resource());
}

namespace {

// Common gateway for the "size-from-scalar" test-matrix functions
// (pascal, hilb, invhilb, wilkinson, hadamard).
size_t requireSizeArg(Span<const Value> args, const char *fn)
{
    if (args.size() != 1)
        throw Error(std::string(fn) + ": requires exactly 1 argument",
                    0, 0, fn, "", std::string("numkit:") + fn + ":nargin");
    const double nd = args[0].toScalar();
    if (nd < 0.0 || nd != std::floor(nd))
        throw Error(std::string(fn) + ": N must be a non-negative integer",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badN");
    return static_cast<size_t>(nd);
}

} // anonymous namespace

void pascal_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = pascal(requireSizeArg(args, "pascal"), ctx.engine->resource());
}

void hilb_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = hilb(requireSizeArg(args, "hilb"), ctx.engine->resource());
}

void invhilb_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = invhilb(requireSizeArg(args, "invhilb"), ctx.engine->resource());
}

void wilkinson_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = wilkinson(requireSizeArg(args, "wilkinson"), ctx.engine->resource());
}

void hadamard_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = hadamard(requireSizeArg(args, "hadamard"), ctx.engine->resource());
}

void rosser_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (!args.empty())
        throw Error("rosser: takes no arguments",
                    0, 0, "rosser", "", "numkit:rosser:nargin");
    outs[0] = rosser(ctx.engine->resource());
}

// NOTE: inv_reg migrated to libs/linalg (properties.cpp).

// NOTE: linsolve_reg migrated to libs/linalg (solvers.cpp).
// NOTE: pageinv_reg  migrated to libs/linalg (page_ops.cpp).

// NOTE: trace_reg / det_reg migrated to libs/linalg (properties.cpp).
// NOTE: chol_reg / lu_reg / qr_reg / svd_reg migrated to
//       libs/linalg (decompositions.cpp).
// NOTE: rank_reg / cond_reg / normest_reg migrated to
//       libs/linalg (properties.cpp).
// NOTE: pinv_reg / orth_reg / null_reg migrated to
//       libs/linalg (pseudo_subspace.cpp).
// NOTE: eig_reg / hess_reg / schur_reg / sylvester_reg /
//       expm_reg / logm_reg / sqrtm_reg migrated to libs/linalg
//       (eig.cpp, matrix_functions.cpp). Block below disabled.

void topkrows_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("topkrows: requires (A, k[, col[, direction]])",
                    0, 0, "topkrows", "", "numkit:topkrows:nargin");
    const double kd = args[1].toScalar();
    if (kd < 0.0 || kd != std::floor(kd))
        throw Error("topkrows: k must be a non-negative integer",
                    0, 0, "topkrows", "", "numkit:topkrows:badK");
    auto *mr = ctx.engine->resource();

    // Parse `col` (positive int / vector) and `direction` (string /
    // string vector). 'ComparisonMethod' NV pair (auto/real/abs) is
    // accept-and-ignore — numkit only supports real numeric input here.
    std::vector<std::size_t> cols;
    std::vector<std::uint8_t> desc;
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        // Treat as col vector.
        const Value &c = args[i];
        cols.reserve(c.numel());
        for (std::size_t kk = 0; kk < c.numel(); ++kk) {
            const double v = c.elemAsDouble(kk);
            if (v < 1.0 || v != std::floor(v))
                throw Error("topkrows: col must be a positive integer "
                            "or vector of positive integers",
                            0, 0, "topkrows", "", "numkit:topkrows:badCol");
            cols.push_back(static_cast<std::size_t>(v) - 1);
        }
        ++i;
    }
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = args[i].toString();
        if (s == "ComparisonMethod") {
            // accept-and-ignore; consume value
            i += 2;
        } else {
            if (s == "descend")      desc.push_back(1);
            else if (s == "ascend")  desc.push_back(0);
            else
                throw Error("topkrows: direction must be 'ascend' or "
                            "'descend'",
                            0, 0, "topkrows", "", "numkit:topkrows:badDir");
            ++i;
        }
    }
    // Optional trailing 'ComparisonMethod' NV (after dir).
    while (i + 1 < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string nm = args[i].toString();
        if (nm == "ComparisonMethod") { i += 2; continue; }
        throw Error("topkrows: unexpected argument '" + nm + "'",
                    0, 0, "topkrows", "", "numkit:topkrows:badArg");
    }

    std::vector<std::size_t> idx_out;
    auto B = topkrows_full(args[0], static_cast<std::size_t>(kd),
                            cols, desc,
                            (nargout >= 2) ? &idx_out : nullptr, mr);
    outs[0] = std::move(B);
    if (nargout >= 2) {
        auto I = Value::matrix(idx_out.size(), 1, ValueType::DOUBLE, mr);
        double *id = I.doubleDataMut();
        for (std::size_t k2 = 0; k2 < idx_out.size(); ++k2)
            id[k2] = double(idx_out[k2] + 1);  // 1-indexed
        outs[1] = std::move(I);
    }
}

void size_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("Not enough input arguments",
                     0, 0, "size", "", "numkit:size:nargin");
    auto *mr = ctx.engine->resource();

    if (args.size() >= 2) {
        outs[0] = size(args[0], static_cast<int>(args[1].toScalar()), mr);
        return;
    }

    if (nargout > 1) {
        const auto &dims = args[0].dims();
        // Multi-output form: [r, c] = size(A) or [r, c, p, ...] = size(A).
        // For ND tensors, dims past nargout-1 are gathered into the last
        // requested output (MATLAB behaviour: extra-dim sizes multiplied
        // into the trailing slot). For dims past actual ndim, return 1.
        for (size_t i = 0; i < nargout && i < outs.size(); ++i) {
            double v;
            if (i + 1 < nargout) {
                v = static_cast<double>(dims.dim(static_cast<int>(i)));
            } else {
                // Last requested output: multiply remaining dims (if any).
                size_t prod = 1;
                for (int j = static_cast<int>(i); j < dims.ndim(); ++j)
                    prod *= dims.dim(j);
                if (dims.ndim() <= static_cast<int>(i)) prod = 1;
                v = static_cast<double>(prod);
            }
            outs[i] = Value::scalar(v, mr);
        }
        return;
    }

    outs[0] = size(args[0], mr);
}

void length_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("length: requires 1 argument",
                     0, 0, "length", "", "numkit:length:nargin");
    outs[0] = length(args[0], ctx.engine->resource());
}

void numel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("numel: requires 1 argument",
                     0, 0, "numel", "", "numkit:numel:nargin");
    outs[0] = numel(args[0], ctx.engine->resource());
}

void ndims_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ndims: requires 1 argument",
                     0, 0, "ndims", "", "numkit:ndims:nargin");
    outs[0] = ndims(args[0], ctx.engine->resource());
}

void reshape_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("reshape: requires at least 2 arguments",
                     0, 0, "reshape", "", "numkit:reshape:nargin");

    const auto &x = args[0];
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> dims(&scratch);

    // Dims-vector form: reshape(A, [m n p ...]). No [] inference here.
    if (args.size() == 2 && !args[1].isScalar() && !args[1].isEmpty()) {
        dims = parseDimsArgsND(&scratch, args.subspan(1));
    } else {
        // Scalar-args form: reshape(A, m, n, ...). One [] allowed for
        // dimension inference from x.numel().
        const size_t dimCount = args.size() - 1;
        dims.assign(dimCount, 1);
        int inferPos = -1;
        size_t knownProd = 1;
        for (size_t i = 0; i < dimCount; ++i) {
            if (args[i + 1].isEmpty()) {
                if (inferPos >= 0)
                    throw Error("reshape: only one dimension may be inferred via []",
                                 0, 0, "reshape", "", "numkit:reshape:tooManyInferred");
                inferPos = static_cast<int>(i);
            } else {
                dims[i] = static_cast<size_t>(args[i + 1].toScalar());
                knownProd *= dims[i];
            }
        }
        if (inferPos >= 0) {
            if (knownProd == 0 || x.numel() % knownProd != 0)
                throw Error("reshape: size of array must be divisible by product of known dims",
                             0, 0, "reshape", "", "numkit:reshape:indivisible");
            dims[inferPos] = x.numel() / knownProd;
        }
    }

    // Strip trailing 1s past the 2nd dim (MATLAB convention).
    stripTrailingOnes(dims);
    outs[0] = reshapeND(x, Span<const size_t>(dims.data(), dims.size()), mr);
}

void transpose_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("transpose: requires 1 argument",
                     0, 0, "transpose", "", "numkit:transpose:nargin");
    outs[0] = transpose(args[0], ctx.engine->resource());
}

void pagetranspose_reg(Span<const Value> args, size_t, Span<Value> outs,
                       CallContext &ctx)
{
    if (args.empty())
        throw Error("pagetranspose: requires 1 argument",
                     0, 0, "pagetranspose", "", "numkit:pagetranspose:nargin");
    outs[0] = pagetranspose(args[0], ctx.engine->resource());
}

void pagectranspose_reg(Span<const Value> args, size_t, Span<Value> outs,
                        CallContext &ctx)
{
    if (args.empty())
        throw Error("pagectranspose: requires 1 argument",
                     0, 0, "pagectranspose", "", "numkit:pagectranspose:nargin");
    outs[0] = pagectranspose(args[0], ctx.engine->resource());
}

void peaks_reg(Span<const Value> args, size_t, Span<Value> outs,
               CallContext &ctx)
{
    size_t n = 49;  // MATLAB default
    if (!args.empty()) {
        const double dn = args[0].toScalar();
        if (dn < 0 || dn > 1.0e9 || std::isnan(dn))
            throw Error("peaks: n must be a non-negative integer",
                         0, 0, "peaks", "", "numkit:peaks:badN");
        n = static_cast<size_t>(dn);
    }
    outs[0] = peaks(n, ctx.engine->resource());
}

void sphere_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    size_t n = 20;  // MATLAB default
    if (!args.empty()) n = static_cast<size_t>(args[0].toScalar());
    auto s = sphere(n, ctx.engine->resource());
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void cylinder_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    size_t n = 20;
    Value R;
    if (args.empty()) {
        // Default profile [1 1].
        R = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        R.doubleDataMut()[0] = 1.0;
        R.doubleDataMut()[1] = 1.0;
    } else {
        if (args[0].isScalar()) {
            // cylinder(n) — single integer arg is `n`, R defaults to [1 1].
            n = static_cast<size_t>(args[0].toScalar());
            R = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            R.doubleDataMut()[0] = 1.0;
            R.doubleDataMut()[1] = 1.0;
        } else {
            R = args[0];
            if (args.size() >= 2)
                n = static_cast<size_t>(args[1].toScalar());
        }
    }
    auto s = cylinder(R, n, mr);
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void ellipsoid_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 6)
        throw Error("ellipsoid: requires (xc, yc, zc, xr, yr, zr [, n])",
                     0, 0, "ellipsoid", "", "numkit:ellipsoid:nargin");
    const double xc = args[0].toScalar();
    const double yc = args[1].toScalar();
    const double zc = args[2].toScalar();
    const double xr = args[3].toScalar();
    const double yr = args[4].toScalar();
    const double zr = args[5].toScalar();
    size_t n = 20;
    if (args.size() >= 7) n = static_cast<size_t>(args[6].toScalar());
    auto s = ellipsoid(xc, yc, zc, xr, yr, zr, n, ctx.engine->resource());
    outs[0] = std::move(s.X);
    if (nargout > 1) outs[1] = std::move(s.Y);
    if (nargout > 2) outs[2] = std::move(s.Z);
}

void pagemtimes_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto parseFlag = [](const Value &v) -> TranspOp {
        if (!v.isChar() && !v.isString())
            throw Error("pagemtimes: transpose flag must be a string",
                         0, 0, "pagemtimes", "", "numkit:pagemtimes:flagType");
        const std::string s = v.toString();
        if (s == "none")       return TranspOp::None;
        if (s == "transpose")  return TranspOp::Transpose;
        if (s == "ctranspose") return TranspOp::CTranspose;
        throw Error("pagemtimes: invalid transpose flag '" + s
                     + "' (expected 'none', 'transpose', or 'ctranspose')",
                     0, 0, "pagemtimes", "", "numkit:pagemtimes:invalidFlag");
    };
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = pagemtimes(args[0], args[1], mr);
        return;
    }
    if (args.size() == 4) {
        outs[0] = pagemtimes(args[0], parseFlag(args[1]), args[2], parseFlag(args[3]), mr);
        return;
    }
    throw Error("pagemtimes: expected 2 or 4 arguments",
                 0, 0, "pagemtimes", "", "numkit:pagemtimes:nargin");
}

void diag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diag: requires 1 argument",
                     0, 0, "diag", "", "numkit:diag:nargin");
    outs[0] = diag(args[0], ctx.engine->resource());
}

void sort_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sort: requires 1 argument",
                     0, 0, "sort", "", "numkit:sort:nargin");
    // sort(X[, dim][, direction]): a numeric trailing arg is the dim, a
    // string is the direction ('ascend' default / 'descend'). MATLAB
    // accepts sort(X,direction) and sort(X,dim,direction).
    int dim = -1;
    bool descend = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].isEmpty()) continue;
        if (args[i].isChar() || args[i].isString()) {
            std::string d = args[i].toString();
            for (char &ch : d) if (ch >= 'A' && ch <= 'Z') ch = char(ch + 32);
            if (d == "descend") descend = true;
            else if (d == "ascend") descend = false;
            // ignore other Name-Value tokens (e.g. ComparisonMethod)
        } else {
            dim = static_cast<int>(args[i].toScalar());
        }
    }
    auto [sorted, idx] = sort(args[0], dim, descend, ctx.engine->resource());
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void sortrows_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sortrows: requires at least 1 argument",
                     0, 0, "sortrows", "", "numkit:sortrows:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto cols = ScratchVec<int>(&scratch);
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const auto &c = args[1];
        if (c.type() == ValueType::CHAR || c.type() == ValueType::STRING)
            throw Error("sortrows: column spec must be numeric",
                         0, 0, "sortrows", "", "numkit:sortrows:badColType");
        cols.reserve(c.numel());
        for (size_t i = 0; i < c.numel(); ++i) {
            const double v = c.elemAsDouble(i);
            if (v != std::floor(v))
                throw Error("sortrows: column index must be an integer",
                             0, 0, "sortrows", "", "numkit:sortrows:badCol");
            cols.push_back(static_cast<int>(v));
        }
    }
    auto [sorted, idx] = sortrows(args[0], Span<const int>(cols.data(), cols.size()), mr);
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void find_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("find: requires 1 argument",
                     0, 0, "find", "", "numkit:find:nargin");
    outs[0] = find(args[0], ctx.engine->resource());
}

void nnz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nnz: requires 1 argument",
                     0, 0, "nnz", "", "numkit:nnz:nargin");
    outs[0] = nnz(args[0], ctx.engine->resource());
}

void nonzeros_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nonzeros: requires 1 argument",
                     0, 0, "nonzeros", "", "numkit:nonzeros:nargin");
    outs[0] = nonzeros(args[0], ctx.engine->resource());
}

void horzcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = horzcat(args, ctx.engine->resource());
}

void vertcat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = vertcat(args, ctx.engine->resource());
}

void meshgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("meshgrid: requires at least 1 argument",
                     0, 0, "meshgrid", "", "numkit:meshgrid:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        // meshgrid(x) ≡ meshgrid(x, x). See BUGS.md #21.
        auto [X, Y] = meshgrid(args[0], args[0], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 2) {
        auto [X, Y] = meshgrid(args[0], args[1], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 3) {
        auto [X, Y, Z] = meshgrid(args[0], args[1], args[2], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        if (nargout > 2) outs[2] = std::move(Z);
        return;
    }
    throw Error("meshgrid: 4+ inputs are not supported",
                 0, 0, "meshgrid", "", "numkit:meshgrid:tooManyInputs");
}

void ndgrid_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ndgrid: requires at least 2 arguments",
                     0, 0, "ndgrid", "", "numkit:ndgrid:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 2) {
        auto [X, Y] = ndgrid(args[0], args[1], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        return;
    }
    if (args.size() == 3) {
        auto [X, Y, Z] = ndgrid(args[0], args[1], args[2], mr);
        outs[0] = std::move(X);
        if (nargout > 1) outs[1] = std::move(Y);
        if (nargout > 2) outs[2] = std::move(Z);
        return;
    }
    throw Error("ndgrid: 4+ inputs are not yet supported",
                 0, 0, "ndgrid", "", "numkit:ndgrid:tooManyInputs");
}

// NOTE: kron_reg migrated to libs/linalg/src/vector_ops.cpp.

// cumsum_reg / cumprod_reg are defined below (after the cum-flag helpers),
// where flip() is in scope — they parse the 'reverse'/'forward' direction
// and 'omitnan'/'includenan' flags like MATLAB.

// MATLAB cummax / cummin accept positional 'reverse' / 'omitnan' /
// 'includenan' string flags after the optional dim. Trick: 'reverse'
// = flip + cum + flip; 'includenan' propagation requires a second pass
// that fills NaN forward from the first NaN onwards (since the cum*
// kernel itself already skips NaN per omitnan default).
namespace {

void parseCumDirNan(Span<const Value> args, size_t start,
                    int &dim, bool &reverse, bool &include_nan)
{
    dim = 0;
    reverse = false;
    include_nan = false;        // matches numkit default = MATLAB default
    size_t i = start;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    while (i < args.size()) {
        if (!(args[i].isChar() || args[i].isString())) {
            throw Error("cummax/cummin: trailing positional must be a string flag",
                        0, 0, "cummax/cummin", "", "numkit:cum:badArg");
        }
        std::string s = args[i].toString();
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "reverse")    reverse = true;
        else if (s == "forward")    reverse = false;
        else if (s == "omitnan")    include_nan = false;
        else if (s == "includenan") include_nan = true;
        else
            throw Error("cummax/cummin: unknown flag '" + s + "'",
                        0, 0, "cummax/cummin", "", "numkit:cum:flag");
        ++i;
    }
}

// Propagate NaN forward in `out` based on the NaN positions in the
// (already same-shape) `src` input. Used to implement 'includenan'
// for cummax/cummin: once a NaN is hit in src along the operating
// dim, every subsequent output entry is set to NaN.
void propagateNanFromSrc(Value &out, const Value &src, int dim1Based)
{
    const auto &dd = out.dims();
    const int nd = dd.ndim();
    const int d = dim1Based;
    if (d < 1 || d > nd) return;
    size_t inner = 1;
    for (int i = 0; i < d - 1; ++i) inner *= dd.dim(i);
    size_t outer = 1;
    for (int i = d; i < nd; ++i) outer *= dd.dim(i);
    const size_t L = dd.dim(d - 1);
    double *o = out.doubleDataMut();
    const double *s = src.doubleData();
    for (size_t oc = 0; oc < outer; ++oc)
        for (size_t b = 0; b < inner; ++b) {
            const size_t base = oc * inner * L + b;
            bool seenNaN = false;
            for (size_t k = 0; k < L; ++k) {
                if (!seenNaN && std::isnan(s[base + k * inner]))
                    seenNaN = true;
                if (seenNaN)
                    o[base + k * inner] = std::numeric_limits<double>::quiet_NaN();
            }
        }
}

template <typename Fn>
Value runCumWithFlags(const Value &x, Span<const Value> args, Fn impl, std::pmr::memory_resource *mr)
{
    int dim; bool reverse; bool include_nan;
    parseCumDirNan(args, 1, dim, reverse, include_nan);
    Value src = x;
    if (reverse) src = flip(src, dim, mr);
    Value out = (dim > 0) ? impl(src, dim, mr) : impl(src, 0, mr);
    if (include_nan) {
        // Determine effective dim (firstNonSingleton when dim=0).
        int effDim = dim;
        if (effDim <= 0) {
            const auto &dd = out.dims();
            effDim = 1;
            for (int k = 0; k < dd.ndim(); ++k)
                if (dd.dim(k) > 1) { effDim = k + 1; break; }
        }
        propagateNanFromSrc(out, src, effDim);
    }
    if (reverse) {
        int effDim = dim;
        if (effDim <= 0) {
            const auto &dd = out.dims();
            effDim = 1;
            for (int k = 0; k < dd.ndim(); ++k)
                if (dd.dim(k) > 1) { effDim = k + 1; break; }
        }
        out = flip(out, effDim, mr);
    }
    return out;
}

} // anonymous

void cummax_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cummax: requires at least 1 argument",
                     0, 0, "cummax", "", "numkit:cummax:nargin");
    outs[0] = runCumWithFlags(args[0], args, [](const Value &v, int d, std::pmr::memory_resource *mr) {
                                  return cummax(v, d, mr);
                              }, ctx.engine->resource());
}

void cummin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cummin: requires at least 1 argument",
                     0, 0, "cummin", "", "numkit:cummin:nargin");
    outs[0] = runCumWithFlags(args[0], args, [](const Value &v, int d, std::pmr::memory_resource *mr) {
                                  return cummin(v, d, mr);
                              }, ctx.engine->resource());
}

namespace {

// cumsum/cumprod option handling. Unlike cummax/cummin (whose kernel skips
// NaN), the cumsum/cumprod kernels PROPAGATE NaN — which is MATLAB's
// 'includenan' default. So 'omitnan' is implemented by replacing NaN with
// the additive/multiplicative identity (0 / 1) BEFORE the scan. 'reverse'
// = flip → scan → flip along the operating dimension.
Value cumScanFlags(const Value &x, Span<const Value> args, bool isProd,
                   std::pmr::memory_resource *mr)
{
    int dim = 0; bool reverse = false; bool omitnan = false;
    size_t i = 1;
    if (i < args.size() && !args[i].isEmpty()
        && !args[i].isChar() && !args[i].isString()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    for (; i < args.size(); ++i) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string s = args[i].toString();
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "reverse")    reverse = true;
        else if (s == "forward")    reverse = false;
        else if (s == "omitnan")    omitnan = true;
        else if (s == "includenan") omitnan = false;
    }
    // Effective (1-based) dim: first non-singleton when unspecified.
    int effDim = dim;
    if (effDim <= 0) {
        const auto &dd = x.dims();
        effDim = 1;
        for (int k = 0; k < dd.ndim(); ++k)
            if (dd.dim(k) > 1) { effDim = k + 1; break; }
    }
    Value src = x;
    if (omitnan && src.type() == ValueType::DOUBLE) {
        const double id = isProd ? 1.0 : 0.0;
        double *d = src.doubleDataMut();
        const size_t n = src.numel();
        for (size_t k = 0; k < n; ++k)
            if (std::isnan(d[k])) d[k] = id;
    }
    if (reverse) src = flip(src, effDim, mr);
    Value out = isProd ? cumprod(src, effDim, mr) : cumsum(src, effDim, mr);
    if (reverse) out = flip(out, effDim, mr);
    return out;
}

} // anonymous

void cumsum_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumsum: requires at least 1 argument",
                     0, 0, "cumsum", "", "numkit:cumsum:nargin");
    outs[0] = cumScanFlags(args[0], args, /*isProd=*/false, ctx.engine->resource());
}

void cumprod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumprod: requires at least 1 argument",
                     0, 0, "cumprod", "", "numkit:cumprod:nargin");
    outs[0] = cumScanFlags(args[0], args, /*isProd=*/true, ctx.engine->resource());
}

void diff_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("diff: requires at least 1 argument",
                     0, 0, "diff", "", "numkit:diff:nargin");
    int n = 1;
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const double nv = args[1].toScalar();
        if (nv != std::floor(nv) || nv < 0)
            throw Error("diff: order n must be a non-negative integer",
                         0, 0, "diff", "", "numkit:diff:badOrder");
        n = static_cast<int>(nv);
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = diff(args[0], n, dim, ctx.engine->resource());
}

#undef NK_CUM_REG

#define NK_LOGICAL_RED_REG(name, fn)                                           \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = fn(args[0], dim, ctx.engine->resource());                   \
    }

NK_LOGICAL_RED_REG(any, anyOf)
NK_LOGICAL_RED_REG(all, allOf)

#undef NK_LOGICAL_RED_REG

void xor_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("xor: requires 2 arguments",
                     0, 0, "xor", "", "numkit:xor:nargin");
    outs[0] = xorOf(args[0], args[1], ctx.engine->resource());
}

// NOTE: cross_reg / dot_reg migrated to libs/linalg/src/vector_ops.cpp.

} // namespace detail

} // namespace numkit::builtin
