// libs/builtin/src/lang/arrays/matrix.cpp

#include <numkit/builtin/language/arrays/matrix.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"
#include "rows_helpers.hpp"
#include <numkit/ops/binary_ops.hpp>
#include <numkit/ops/la_solve.hpp>
#include "math/arithmetic/cumsum.hpp"
#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/builtin/language/arrays/manip.hpp>     // flip()
#include <numkit/builtin/language/operators/unary_ops.hpp>  // transposeNC()

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

#include "matrix_detail.hpp"

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


Value toeplitz(const Value &cV, const Value &rV, std::pmr::memory_resource *mr)
{
    const bool single1 = rV.isEmpty();
    const bool anyCplx  = cV.isComplex() || (!single1 && rV.isComplex());
    const bool anySingle = !anyCplx &&
        (cV.type() == ValueType::SINGLE ||
         (!single1 && rV.type() == ValueType::SINGLE));

    const std::size_t m = cV.numel();
    const std::size_t n = single1 ? m : rV.numel();
    if (m == 0 || n == 0)
        throw Error("toeplitz: inputs must be non-empty",
                    0, 0, "toeplitz", "", "numkit:toeplitz:empty");

    // COMPLEX: gather per-element so the imaginary part is preserved.
    // Two-arg form is a plain gather; the single-arg complex form is a
    // Hermitian Toeplitz matrix (MATLAB conjugates the strictly-lower
    // triangle, T(i,j) = conj(c(i-j)) for i>j, c(j-i) otherwise).
    if (anyCplx) {
        ScratchArena scratch(mr);
        ScratchVec<Complex> c(m, &scratch);
        for (size_t k = 0; k < m; ++k)
            c[k] = cV.isComplex() ? cV.complexData()[k]
                                  : Complex(cV.elemAsDouble(k), 0.0);
        ScratchVec<Complex> r(n, &scratch);
        if (!single1)
            for (size_t k = 0; k < n; ++k)
                r[k] = rV.isComplex() ? rV.complexData()[k]
                                      : Complex(rV.elemAsDouble(k), 0.0);
        auto M = Value::matrix(m, n, ValueType::COMPLEX, mr);
        Complex *dst = M.complexDataMut();
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < m; ++i) {
                Complex v;
                if (single1) v = (i > j) ? std::conj(c[i - j]) : c[j - i];
                else         v = (i >= j) ? c[i - j] : r[j - i];
                dst[j * m + i] = v;
            }
        return M;
    }

    // Real path (DOUBLE or SINGLE output; class preserved).
    ScratchArena scratch(mr);
    auto c = valueToScratchDoubles(cV, scratch);
    ScratchVec<double> r(&scratch);
    if (single1) r.assign(c.begin(), c.end());
    else         r = valueToScratchDoubles(rV, scratch);

    // T[i,j] = c[i-j] (i>=j) else r[j-i]. MATLAB silently overrides r[0]
    // with c[0] when both are given (r[0] is never read here).
    const ValueType ot = anySingle ? ValueType::SINGLE : ValueType::DOUBLE;
    auto M = Value::matrix(m, n, ot, mr);
    if (ot == ValueType::SINGLE) {
        float *dst = static_cast<float *>(M.rawDataMut());
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < m; ++i)
                dst[j * m + i] = static_cast<float>((i >= j) ? c[i - j] : r[j - i]);
    } else {
        double *dst = M.doubleDataMut();
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < m; ++i)
                dst[j * m + i] = (i >= j) ? c[i - j] : r[j - i];
    }
    return M;
}

Value hankel(const Value &cV, const Value &rV, std::pmr::memory_resource *mr)
{
    const bool single1 = rV.isEmpty();
    const bool anyCplx  = cV.isComplex() || (!single1 && rV.isComplex());
    const bool anySingle = !anyCplx &&
        (cV.type() == ValueType::SINGLE ||
         (!single1 && rV.type() == ValueType::SINGLE));

    const std::size_t m = cV.numel();
    const std::size_t n = single1 ? m : rV.numel();
    if (m == 0 || n == 0)
        throw Error("hankel: inputs must be non-empty",
                    0, 0, "hankel", "", "numkit:hankel:empty");

    // H[i,j] = c[i+j] (i+j < m) else r[i+j-m+1]. hankel never conjugates.
    // Single-arg: r is all zeros (anti-triangular Hankel).
    if (anyCplx) {
        ScratchArena scratch(mr);
        ScratchVec<Complex> c(m, &scratch);
        for (size_t k = 0; k < m; ++k)
            c[k] = cV.isComplex() ? cV.complexData()[k]
                                  : Complex(cV.elemAsDouble(k), 0.0);
        ScratchVec<Complex> r(n, &scratch);
        for (size_t k = 0; k < n; ++k)
            r[k] = (!single1 && rV.isComplex()) ? rV.complexData()[k]
                 : (!single1)                   ? Complex(rV.elemAsDouble(k), 0.0)
                                                : Complex(0.0, 0.0);
        auto M = Value::matrix(m, n, ValueType::COMPLEX, mr);
        Complex *dst = M.complexDataMut();
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < m; ++i) {
                const size_t s = i + j;
                dst[j * m + i] = (s < m) ? c[s] : r[s - m + 1];
            }
        return M;
    }

    ScratchArena scratch(mr);
    auto c = valueToScratchDoubles(cV, scratch);
    ScratchVec<double> r(&scratch);
    if (single1) r.assign(m, 0.0);
    else         r = valueToScratchDoubles(rV, scratch);

    const ValueType ot = anySingle ? ValueType::SINGLE : ValueType::DOUBLE;
    auto M = Value::matrix(m, n, ot, mr);
    if (ot == ValueType::SINGLE) {
        float *dst = static_cast<float *>(M.rawDataMut());
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < m; ++i) {
                const size_t s = i + j;
                dst[j * m + i] = static_cast<float>((s < m) ? c[s] : r[s - m + 1]);
            }
    } else {
        double *dst = M.doubleDataMut();
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < m; ++i) {
                const size_t s = i + j;
                dst[j * m + i] = (s < m) ? c[s] : r[s - m + 1];
            }
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

    // OBJECT arrays store per-element state, column-major — reshape just
    // changes the shape (element order preserved), like CELL.
    if (x.isObject()) {
        Dims nd = (d.pages > 0) ? Dims(d.rows, d.cols, d.pages) : Dims(d.rows, d.cols);
        return x.objectReshape(nd, mr);
    }

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

    if (x.isObject())
        return x.objectReshape(Dims(dims.data(), static_cast<int>(nDims)), mr);

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
    // The named transpose() builtin is identical to the `.'` operator:
    // type-preserving (CHAR / LOGICAL / SINGLE / int / COMPLEX / CELL).
    // Delegate to transposeNC (unary_ops.cpp) so there is one
    // implementation. The DOUBLE-only path here previously coerced every
    // input to a DOUBLE matrix of element codes.
    return transposeNC(x, mr);
}

// ── pagetranspose / pagectranspose ───────────────────────────────────

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
    return diag(x, 0L, mr);
}

Value diag(const Value &x, long k, std::pmr::memory_resource *mr)
{
    const ValueType t = x.type();
    // MATLAB: "Inputs must be numeric, char, or logical." Cells / strings
    // / structs / function handles are unsupported.
    if (t == ValueType::CELL || t == ValueType::STRING ||
        t == ValueType::STRUCT || t == ValueType::FUNC_HANDLE)
        throw Error("diag: inputs must be numeric, char, or logical",
                     0, 0, "diag", "", "numkit:diag:badType");

    const size_t es = elementSize(t);
    const size_t ak = static_cast<size_t>(k < 0 ? -k : k);

    if (x.dims().isVector()) {
        // Build a square matrix with x on the k-th diagonal. The output
        // matrix is zero-filled (= 0 / char(0) / false / 0+0i), so only
        // the diagonal entries need to be written.
        const size_t n = x.numel();
        const size_t m = n + ak;
        auto r = Value::matrix(m, m, t, mr);
        const char *src = static_cast<const char *>(x.rawData());
        char *dst = static_cast<char *>(r.rawDataMut());
        for (size_t i = 0; i < n; ++i) {
            const size_t row = (k >= 0) ? i : i + ak;
            const size_t col = (k >= 0) ? i + ak : i;
            std::memcpy(dst + (col * m + row) * es, src + i * es, es);  // col-major
        }
        return r;
    }

    // Matrix input: extract the k-th diagonal as a column vector.
    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t row0 = (k >= 0) ? 0 : ak;
    const size_t col0 = (k >= 0) ? ak : 0;
    size_t len = 0;
    if (row0 < R && col0 < C)
        len = std::min(R - row0, C - col0);
    auto r = Value::matrix(len, 1, t, mr);
    const char *src = static_cast<const char *>(x.rawData());
    char *dst = static_cast<char *>(r.rawDataMut());
    for (size_t i = 0; i < len; ++i) {
        const size_t row = row0 + i, col = col0 + i;
        std::memcpy(dst + i * es, src + (col * R + row) * es, es);  // x col-major
    }
    return r;
}

// ── Sort / find ──────────────────────────────────────────────────────
// Complex sort (MATLAB rule): order by magnitude |z| ascending; ties broken
// by phase angle arg(z) in (-pi, pi] ascending. A NaN component (|z| = NaN)
// sorts LAST for ascending, FIRST for descending. 'descend' fully reverses
// both keys. Unlike min/max there is NO all-real fast path — a COMPLEX-typed
// all-real input still sorts by |z|+angle (so sort([2 -2]+0i) = [2 -2]).
std::tuple<Value, Value> sortComplex(const Value &x, int dim, bool descend,
                                     NanPlace nanPlace,
                                     std::pmr::memory_resource *mr)
{
    // NaN side: Auto = first for descending / last for ascending; First/Last
    // force the side regardless of direction (MATLAB 'MissingPlacement').
    const bool nanFirst = (nanPlace == NanPlace::Auto) ? descend
                                                       : (nanPlace == NanPlace::First);
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
                          [descend, nanFirst](const auto &a, const auto &b) {
                              const double am = std::abs(a.first), bm = std::abs(b.first);
                              const bool an = std::isnan(am), bn = std::isnan(bm);
                              if (an || bn) {
                                  if (an && bn) return false;
                                  return nanFirst ? an : bn;
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
                              NanPlace nanPlace, std::pmr::memory_resource *mr)
{
    if (x.isScalar())
        return std::make_tuple(x, Value::scalar(1.0, mr));
    if (x.type() == ValueType::COMPLEX)
        return sortComplex(x, dim, descend, nanPlace, mr);

    // NaN side: Auto = first for descending / last for ascending; First/Last
    // force the side regardless of direction (MATLAB 'MissingPlacement').
    const bool nanFirst = (nanPlace == NanPlace::Auto) ? descend
                                                       : (nanPlace == NanPlace::First);

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
                          [descend, nanFirst](const auto &a, const auto &b) {
                              const double av = a.first, bv = b.first;
                              const bool an = std::isnan(av), bn = std::isnan(bv);
                              if (an || bn) {
                                  if (an && bn) return false;
                                  return nanFirst ? an : bn;
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
        return Value();
    return Value::horzcat(values.data(), values.size(), mr);
}

Value vertcat(Span<const Value> values, std::pmr::memory_resource *mr)
{
    if (values.empty())
        return Value();
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


Value cumsum(const Value &x, std::pmr::memory_resource *mr)
{
    if (isIntegerType(x.type()))
        return cumIntegerNative(x, 0, /*isProd=*/false, mr);
    if (x.type() == ValueType::COMPLEX)
        return cumComplexAlongDim(x, firstNonSingletonDim(x), /*isProd=*/false, mr);
    // MATLAB promotes logical → double for cumsum (char/string error out and
    // keep doing so via the doubleData() path below — only logical is valid).
    if (x.isLogical())
        return cumsum(toDoubleValue(x, mr), mr);
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
    if (x.type() == ValueType::COMPLEX)
        return cumComplexAlongDim(x, dim <= 0 ? firstNonSingletonDim(x) : dim,
                                  /*isProd=*/false, mr);
    if (x.isLogical())
        return cumsum(toDoubleValue(x, mr), dim, mr);
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

// cumprod / cummax / cummin: SIMD prefix-op kernels in
// backends/MStdCumSum_{simd,portable}.cpp handle vector input and the
// dim=1 (column) path where access is contiguous. For dim=2/3 the
// strided access pattern doesn't benefit from SIMD; cumImpl's scalar
// cumKernel still handles those (with the same Op as before).

Value cumprod(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (isIntegerType(x.type()))
        return cumIntegerNative(x, dim, /*isProd=*/true, mr);
    if (x.type() == ValueType::COMPLEX)
        return cumComplexAlongDim(x, dim <= 0 ? firstNonSingletonDim(x) : dim,
                                  /*isProd=*/true, mr);
    if (x.isLogical())
        return cumprod(toDoubleValue(x, mr), dim, mr);
    return cumScanDispatch(x, dim, cumprodScan, [](double a, double b) { return a * b; }, "cumprod", mr);
}


Value cummax(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (x.isLogical())
        return logicalizeCumResult(cummax(toDoubleValue(x, mr), dim, mr), mr);
    // Integer: MATLAB cummax/cummin PRESERVE the class (order statistics, so
    // the result is a subset of the input — the double round-trip is exact).
    if (isIntegerType(x.type()))
        return doubleToIntegerExact(cummax(toDoubleValue(x, mr), dim, mr), x.type(), mr);
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
    if (x.isLogical())
        return logicalizeCumResult(cummin(toDoubleValue(x, mr), dim, mr), mr);
    if (isIntegerType(x.type()))
        return doubleToIntegerExact(cummin(toDoubleValue(x, mr), dim, mr), x.type(), mr);
    return cumScanDispatch(x, dim, cumminScan, [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::min(a, b);
                           }, "cummin", mr);
}

// ── diff: discrete difference ────────────────────────────────────────

Value diff(const Value &x, int n, int dim, std::pmr::memory_resource *mr)
{
    // MATLAB: the difference order N must be a positive integer scalar; n == 0
    // (identity) and negative orders are errors. The reg validates the
    // user-facing path; this guards the C++ primitive too (no internal caller
    // passes 0).
    if (n < 1)
        throw Error("diff: Difference order N must be a positive integer scalar",
                     0, 0, "diff", "", "numkit:diff:badOrder");

    const bool isInt = isIntegerType(x.type());

    // Scalar: MATLAB returns 1×0 empty (class preserved for integer input).
    if (x.isScalar())
        return Value::matrix(1, 0, isInt ? x.type() : ValueType::DOUBLE, mr);

    const int d = detail::resolveDim(x, dim, "diff");
    const auto &dd = x.dims();
    const size_t sliceLen = (d >= 1 && d <= dd.ndim()) ? dd.dim(d - 1) : 1;

    // If n collapses or exceeds the dim, return correctly-shaped empty
    // (preserving the integer class for integer input).
    if (sliceLen <= static_cast<size_t>(n)) {
        if (isInt) {
            const int nd = dd.ndim();
            size_t outDims[Dims::kMaxRank];
            for (int i = 0; i < nd; ++i) outDims[i] = dd.dim(i);
            outDims[d - 1] = 0;
            return Value::matrixND(outDims, nd, x.type(), mr);
        }
        return makeDiffOutput(dd, d, sliceLen, ValueType::DOUBLE, mr);
    }

    // Integer types: keep the class and saturate at each pass (MATLAB).
    if (isInt)
        return diffInteger(x, n, d, mr);

    // Complex: difference real and imaginary parts together (MATLAB).
    if (x.type() == ValueType::COMPLEX) {
        Value cur = copyComplexSameShape(x, mr);
        for (int pass = 0; pass < n; ++pass) {
            const auto &curDims = cur.dims();
            auto out = makeDiffOutput(curDims, d, 1, ValueType::COMPLEX, mr);
            diffOnceComplex(cur.complexData(), out.complexDataMut(), curDims, d);
            cur = std::move(out);
        }
        return cur;
    }

    // Promote logical to DOUBLE first.
    Value cur = copyToDouble(x, mr);

    for (int pass = 0; pass < n; ++pass) {
        const auto &curDims = cur.dims();
        auto out = makeDiffOutput(curDims, d, 1, ValueType::DOUBLE, mr);
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

} // namespace numkit::builtin
