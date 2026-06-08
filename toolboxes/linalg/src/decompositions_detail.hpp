// toolboxes/linalg/src/decompositions_detail.hpp
//
// PRIVATE (src-level) header — NOT part of the public linalg API.
//
// These are raw-buffer / low-level factorisation kernels shared between the
// engine-free compute (decompositions.cpp) and its register half
// (decompositions_reg.cpp). They are kept out of the public
// include/numkit/linalg/decompositions.hpp because their signatures take raw
// `double *` buffers / out-params, which LIBRARY_API.md forbids on the public
// surface. The user-facing builtins (chol / lu / qr / svd …) keep their clean
// Value signatures in the public header; this header only exposes the shared
// internals the CallContext wrappers reach into (the no-throw [R,p] chol form
// needs the failure column; the [Q,R,P] qr form needs the pivot permutation).
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <tuple>
#include <vector>

#include <numkit/value/value.hpp>

namespace numkit::linalg {

// Build upper-triangular R (column-major, n×n) such that R'·R = A, reading the
// upper triangle of A. Returns the 1-based column index where the
// factorization broke down (non-positive pivot), or 0 on success. On failure
// the leading (return−1)×(return−1) block of r is a valid factor. Backs both
// the throwing `chol` and the no-throw `[R,p] = chol(A)` register form.
std::size_t cholUpperFactor(const double *a, double *r, std::size_t n);

// Transpose a square k×k column-major matrix into a fresh Value (turns the
// upper factor R into the lower factor L = R' for `chol(A,'lower')`).
Value transposeSquare(const double *src, std::size_t k,
                      std::pmr::memory_resource *mr);

// Column-pivoted QR (the [Q,R,P] form). Returns Q (m×m) and R (m×n) and fills
// `perm` (0-based, length n) with the column permutation: A·P = Q·R where
// column k of A·P is original column perm[k].
std::tuple<Value, Value>
qr_pivoted(const Value &A, std::vector<std::size_t> &perm,
           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
