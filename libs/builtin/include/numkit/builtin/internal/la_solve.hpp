// libs/builtin/include/numkit/builtin/internal/la_solve.hpp
//
// Internal kernel: solve `A · X = B` (LU for square A, Householder QR
// for tall A). Backs both:
//   - the matrix `\` operator in libs/builtin (mldivide / mrdivide)
//   - user-facing wrappers (`inv`, `linsolve`, `pageinv`, …) in
//     libs/linalg.
//
// `internal/` is a public header but namespaced under
// `numkit::builtin::detail` to signal "implementation detail, not API
// stability promise — only callable from sibling code with reason to
// peer inside builtin's solver."  libs/linalg is exactly such a
// sibling; the layering rule (builtin MUST NOT include linalg) means
// the kernel can only live here, not in libs/linalg.
//
// Wide systems (m < n) are NOT supported here — MATLAB's mldivide
// returns the minimum-norm solution for wide matrices, which requires
// either SVD or QR-of-A'. That path is a separate (deferred) ТЗ.
//
// All scratch goes through std::pmr::memory_resource (HARD RULE — see
// CLAUDE.md "PMR + ScratchArena is a HARD rule across libs/").

#pragma once

#include <cstddef>
#include <memory_resource>

namespace numkit::builtin::detail {

// Solve A · X = B for X.
//   A : m×n column-major (m >= n required; wide systems return false)
//   B : m×nrhs column-major
//   X : n×nrhs column-major (caller-allocated)
//
// Returns false on:
//   - m < n (unsupported)
//   - singular A (square case, zero pivot)
//   - rank-deficient A (tall case, zero column norm or zero R diagonal)
//
// Internal scratch is allocated via the supplied memory_resource (typically
// the per-call ScratchArena from the callsite).
bool la_solve(const double *A, std::size_t m, std::size_t n,
              const double *B, std::size_t nrhs,
              double *X,
              std::pmr::memory_resource *mr);

} // namespace numkit::builtin::detail
