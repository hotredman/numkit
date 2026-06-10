// toolboxes/builtin/include/numkit/builtin/language/arrays/nd_manip.hpp
//
// Phase-6 N-D array manipulation: permute / ipermute / squeeze /
// cat(dim, ...) / blkdiag / shiftdim.

#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <cstddef>

namespace numkit::lang {

/// @brief Permute array dimensions (`B = permute(A, perm)`).
///
/// `perm` is a 1-based permutation of `[1..ndims(A)]`. For a matrix
/// `permute(A, [2 1])` is equivalent to the transpose.
///
/// @param x     Input array.
/// @param perm  Permutation vector (1-based).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Permuted array.
/// @throws Error  Invalid `perm` (not a permutation of `1..ndims`).
/// @see ipermute, squeeze
Value permute(const Value &x, Span<const int> perm,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse permute (`B = ipermute(A, perm)`).
///
/// `ipermute(permute(A, p), p) == A` for any valid `p`.
///
/// @param x     Input array.
/// @param perm  Permutation that was applied originally.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Inverse-permuted array.
/// @see permute
Value ipermute(const Value &x, Span<const int> perm,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Drop singleton dimensions (`B = squeeze(A)`).
///
/// Vectors and 2-D matrices are returned unchanged (squeeze does
/// not act below 2-D); higher-rank arrays with singleton dims collapse
/// down.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Squeezed array.
/// @see shiftdim, permute
Value squeeze(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenate along `dim` (`C = cat(dim, A, B, …)`).
///
/// `dim = 1` is vertcat, `dim = 2` is horzcat, `dim = 3` stacks
/// matrices into a 3-D array (or extends an existing 3-D's page
/// count). Higher `dim` is dispatched to the ND fallback. All inputs
/// must agree on the non-`dim` dimensions.
///
/// @param dim     1-based concatenation dimension.
/// @param values  Inputs to concatenate.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Concatenated array.
/// @throws Error  Dimension mismatch (`m:cat:badDims`) or bad `dim`.
/// @see permute
Value cat(int dim, Span<const Value> values,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Block-diagonal matrix (`B = blkdiag(A1, A2, …)`).
///
/// Places each input on the diagonal of a larger matrix with zeros
/// elsewhere. 2-D inputs only.
///
/// @param values  Diagonal blocks (each 2-D).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Block-diagonal matrix.
/// @throws Error  Any input is not 2-D (`m:blkdiag:notMatrix`).
Value blkdiag(Span<const Value> values,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Cyclic left shift of dim ordering (`B = shiftdim(A, n)`).
///
/// For `n > 0`: equivalent to `permute(A, [n+1, …, N, 1, …, n])`.
/// For `n < 0`: prepends `|n|` singleton dimensions. `n` is reduced
/// `mod N` when it equals or exceeds `N`.
///
/// @param x   Input array.
/// @param n   Shift amount (signed).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Dimension-shifted array.
/// @see shiftdimAuto, permute
Value shiftdim(const Value &x, int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of the auto-form @ref shiftdimAuto.
struct ShiftDimAuto {
    Value v;       ///< Array with leading singletons dropped.
    int dropped;   ///< Number of leading singleton dims removed.
};

/// @brief Auto-shift: drop leading singletons (`[B, k] = shiftdim(A)`).
///
/// The no-`n` form. Returns the array with leading singleton
/// dimensions stripped and the count `k` of dimensions dropped.
/// If no leading singletons, returns `{A, 0}`.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(B, k)` struct.
/// @see shiftdim
ShiftDimAuto shiftdimAuto(const Value &x,
                          std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::lang
