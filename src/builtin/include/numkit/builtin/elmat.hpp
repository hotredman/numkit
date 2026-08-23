// src/builtin/include/numkit/builtin/elmat.hpp
//
// Pure C++ Elementary matrices and array manipulation functions (MATLAB parity).
#pragma once

#include <memory_resource>
#include <string>
#include <tuple>
#include <vector>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @brief Elementary matrices and array manipulation functions (MATLAB parity).
///
/// Provides a clean, engine-free C++ API for generating elementary matrices,
/// reshaping, reordering, padding, splitting, and multidimensional array manipulations.

// ── Multi-value Return Types ───────────────────────────────────────────────

/// @brief Result of 2D grid generation via meshgrid/ndgrid.
struct Meshgrid2D {
    Value X;
    Value Y;
};

/// @brief Result of 3D grid generation via meshgrid/ndgrid.
struct Meshgrid3D {
    Value X;
    Value Y;
    Value Z;
};

// ── Array Creation ──────────────────────────────────────────────────────────

/// @brief Creates an array filled with zeros.
/// @param dims Dimensions of the target matrix or N-D array.
/// @param dtype Data type of elements (default: ValueType::DOUBLE).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Array filled with zeros of the requested shape and type.
/// @code
/// auto Z = numkit::builtin::zeros({3, 3});
/// @endcode
/// @see ones, eye
Value zeros(Span<const size_t> dims, ValueType dtype = ValueType::DOUBLE, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a 2D matrix of zeros.
/// @param rows Number of rows.
/// @param cols Number of columns.
/// @param dtype Data type (default: ValueType::DOUBLE).
/// @param mr Memory resource.
/// @return `rows x cols` matrix of zeros.
Value zeros(size_t rows, size_t cols, ValueType dtype = ValueType::DOUBLE, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an array filled with ones.
/// @param dims Dimensions of the target matrix or N-D array.
/// @param dtype Data type of elements (default: ValueType::DOUBLE).
/// @param mr Memory resource.
/// @return Array filled with ones of the requested shape and type.
/// @see zeros, eye
Value ones(Span<const size_t> dims, ValueType dtype = ValueType::DOUBLE, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a 2D matrix of ones.
/// @param rows Number of rows.
/// @param cols Number of columns.
/// @param dtype Data type (default: ValueType::DOUBLE).
/// @param mr Memory resource.
/// @return `rows x cols` matrix of ones.
Value ones(size_t rows, size_t cols, ValueType dtype = ValueType::DOUBLE, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an identity matrix of size `n x n`.
/// @param n Number of rows and columns.
/// @param dtype Data type (default: ValueType::DOUBLE).
/// @param mr Memory resource.
/// @return `n x n` identity matrix.
Value eye(size_t n, ValueType dtype = ValueType::DOUBLE, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an identity matrix of size `m x n`.
/// @param m Number of rows.
/// @param n Number of columns.
/// @param dtype Data type (default: ValueType::DOUBLE).
/// @param mr Memory resource.
/// @return `m x n` matrix with ones on the main diagonal and zeros elsewhere.
Value eye(size_t m, size_t n, ValueType dtype = ValueType::DOUBLE, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates linearly spaced vector between `a` and `b`.
/// @param a Start point.
/// @param b End point.
/// @param n Number of points (default: 100).
/// @param mr Memory resource.
/// @return Row vector of `n` evenly spaced points.
/// @see logspace
Value linspace(double a, double b, size_t n = 100, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates logarithmically spaced vector between `10^a` and `10^b`.
/// @param a Exponent for start point (`10^a`).
/// @param b Exponent for end point (`10^b` or `pi` if `b == pi`).
/// @param n Number of points (default: 50).
/// @param mr Memory resource.
/// @return Row vector of `n` logarithmically spaced points.
/// @see linspace
Value logspace(double a, double b, size_t n = 50, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an n-by-n magic square.
/// @param n Order of the magic square (n >= 3).
/// @param mr Memory resource.
/// @return `n x n` magic square matrix.
Value magic(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Hilbert matrix of order `n`.
/// @param n Order of the Hilbert matrix.
/// @param mr Memory resource.
/// @return `n x n` matrix where `H(i,j) = 1 / (i + j - 1)`.
/// @see invhilb
Value hilb(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an exact inverse Hilbert matrix of order `n`.
/// @param n Order of the matrix (n <= 14).
/// @param mr Memory resource.
/// @return `n x n` integer inverse Hilbert matrix.
/// @see hilb
Value invhilb(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Pascal matrix of order `n`.
/// @param n Order of the matrix.
/// @param k Form selector (0=symmetric, 1=lower triangular, 2=upper triangular Cholesky factor).
/// @param mr Memory resource.
/// @return `n x n` Pascal matrix.
Value pascal(size_t n, int k = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Toeplitz matrix.
/// @param c First column vector.
/// @param r First row vector (optional, defaults to `c`).
/// @param mr Memory resource.
/// @return Toeplitz matrix formed from `c` and `r`.
Value toeplitz(const Value &c, const Value &r = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Vandermonde matrix.
/// @param v Input vector of points.
/// @param mr Memory resource.
/// @return Vandermonde matrix whose columns are powers of `v`.
Value vander(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Wilkinson eigenvalue test matrix.
/// @param n Order of the matrix (must be odd).
/// @param mr Memory resource.
/// @return `n x n` tridiagonal Wilkinson matrix.
Value wilkinson(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates Rosser's test matrix (8x8 symmetric with exact eigenvalues).
/// @param mr Memory resource.
/// @return 8x8 Rosser matrix.
Value rosser(std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Hadamard matrix of order `n`.
/// @param n Order of the matrix (1, 2, or a multiple of 4).
/// @param mr Memory resource.
/// @return `n x n` Hadamard matrix.
Value hadamard(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Hankel matrix.
/// @param c First column vector.
/// @param r Last row vector (optional, elements beyond `c` default to zero).
/// @param mr Memory resource.
/// @return Hankel matrix.
Value hankel(const Value &c, const Value &r = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Companion matrix of a polynomial.
/// @param p Vector of polynomial coefficients.
/// @param mr Memory resource.
/// @return Companion matrix whose eigenvalues are roots of `p`.
Value compan(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D meshgrid (`[X, Y] = meshgrid(x, y)`).
/// @param x x-axis grid coordinates.
/// @param y y-axis grid coordinates.
/// @param mr Memory resource.
/// @return `(X, Y)` grid pair of size `numel(y) x numel(x)`.
Meshgrid2D meshgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D symmetric meshgrid (`[X, Y] = meshgrid(x)`).
/// @param x Grid coordinates for both axes.
/// @param mr Memory resource.
/// @return `(X, Y)` grid pair.
Meshgrid2D meshgrid(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D meshgrid (`[X, Y, Z] = meshgrid(x, y, z)`).
/// @param x x-axis grid.
/// @param y y-axis grid.
/// @param z z-axis grid.
/// @param mr Memory resource.
/// @return `(X, Y, Z)` grid triple of size `numel(y) x numel(x) x numel(z)`.
Meshgrid3D meshgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D N-D grid (`[X, Y] = ndgrid(x, y)`).
/// @param x First-axis grid.
/// @param y Second-axis grid.
/// @param mr Memory resource.
/// @return `(X, Y)` grid pair of size `numel(x) x numel(y)`.
Meshgrid2D ndgrid(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D N-D grid (`[X, Y, Z] = ndgrid(x, y, z)`).
/// @param x First-axis grid.
/// @param y Second-axis grid.
/// @param z Third-axis grid.
/// @param mr Memory resource.
/// @return `(X, Y, Z)` grid triple of size `numel(x) x numel(y) x numel(z)`.
Meshgrid3D ndgrid(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr = nullptr);

// ── Array Manipulation & Reshaping ──────────────────────────────────────────

/// @brief Replicates and tiles an array.
/// @param x Input array.
/// @param reps Repetition factors along each dimension.
/// @param mr Memory resource.
/// @return Tiled array.
/// @see repelem, reshape
Value repmat(const Value &x, Span<const size_t> reps, std::pmr::memory_resource *mr = nullptr);

/// @brief 2D convenience overload for repmat.
/// @param x Input array.
/// @param r Row repetition factor.
/// @param c Column repetition factor.
/// @param mr Memory resource.
/// @return Tiled array.
Value repmat(const Value &x, size_t r, size_t c, std::pmr::memory_resource *mr = nullptr);

/// @brief Repeats elements of an array.
/// @param x Input array.
/// @param reps Repetition factors per dimension.
/// @param mr Memory resource.
/// @return Array with repeated elements.
Value repelem(const Value &x, Span<const size_t> reps, std::pmr::memory_resource *mr = nullptr);

/// @brief Reshapes array to new dimensions with the same total number of elements.
/// @param x Input array.
/// @param newDims Target shape.
/// @param mr Memory resource.
/// @return Reshaped array (shares buffer COW when possible).
Value reshape(const Value &x, Span<const size_t> newDims, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts diagonal or builds a diagonal matrix.
/// @param x Vector or matrix.
/// @param k Diagonal offset (0=main, >0=above, <0=below).
/// @param mr Memory resource.
/// @return Diagonal vector or diagonal matrix.
/// @see blkdiag
Value diag(const Value &x, int k = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs a block diagonal matrix from input matrices.
/// @param matrices List of matrices to place on the diagonal.
/// @param mr Memory resource.
/// @return Block diagonal matrix.
Value blkdiag(Span<const Value> matrices, std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenates arrays along dimension `dim`.
/// @param dim 1-based dimension (1=rows, 2=cols, 3=pages, etc.).
/// @param arrays List of arrays to concatenate.
/// @param mr Memory resource.
/// @return Concatenated array.
Value cat(int dim, Span<const Value> arrays, std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenates arrays horizontally (along columns, dim 2).
/// @param arrays List of arrays.
/// @param mr Memory resource.
/// @return Horizontally concatenated array.
Value horzcat(Span<const Value> arrays, std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenates arrays vertically (along rows, dim 1).
/// @param arrays List of arrays.
/// @param mr Memory resource.
/// @return Vertically concatenated array.
Value vertcat(Span<const Value> arrays, std::pmr::memory_resource *mr = nullptr);

/// @brief Rotates a 2D matrix by 90 degrees counterclockwise `k` times.
/// @param x Input 2D matrix.
/// @param k Number of 90-degree rotations (default: 1).
/// @param mr Memory resource.
/// @return Rotated matrix.
Value rot90(const Value &x, int k = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Flips matrix left-to-right (along columns).
/// @param x Input 2D matrix.
/// @param mr Memory resource.
/// @return Flipped matrix.
Value fliplr(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Flips matrix up-to-down (along rows).
/// @param x Input 2D matrix.
/// @param mr Memory resource.
/// @return Flipped matrix.
Value flipud(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Reverses the order of elements along specified dimension.
/// @param x Input array.
/// @param dim Dimension to flip along (0 = first non-singleton dimension).
/// @param mr Memory resource.
/// @return Flipped array.
Value flip(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Circularly shifts elements of an array.
/// @param x Input array.
/// @param shifts Shift amount per dimension.
/// @param mr Memory resource.
/// @return Shifted array.
Value circshift(const Value &x, Span<const int> shifts, std::pmr::memory_resource *mr = nullptr);

/// @brief Permutes dimensions of an N-D array.
/// @param x Input array.
/// @param order New ordering of dimensions (1-based).
/// @param mr Memory resource.
/// @return Permuted array.
/// @see ipermute
Value permute(const Value &x, Span<const size_t> order, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse permutes dimensions of an N-D array.
/// @param x Input array.
/// @param order Ordering of dimensions used in previous `permute` call.
/// @param mr Memory resource.
/// @return Array with dimensions restored.
Value ipermute(const Value &x, Span<const size_t> order, std::pmr::memory_resource *mr = nullptr);

/// @brief Shifts dimensions of an N-D array.
/// @param x Input array.
/// @param n Number of positions to shift dimensions (positive: left, negative: right).
/// @param mr Memory resource.
/// @return Array with shifted dimensions.
Value shiftdim(const Value &x, int n = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Removes singleton dimensions from an array.
/// @param x Input array.
/// @param mr Memory resource.
/// @return Squeezed array.
Value squeeze(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Returns the top `k` rows or elements of an array.
/// @param x Input array.
/// @param k Number of rows/elements (default: 8).
/// @param mr Memory resource.
/// @return Subarray containing top elements.
/// @see tail
Value head(const Value &x, size_t k = 8, std::pmr::memory_resource *mr = nullptr);

/// @brief Returns the bottom `k` rows or elements of an array.
/// @param x Input array.
/// @param k Number of rows/elements (default: 8).
/// @param mr Memory resource.
/// @return Subarray containing bottom elements.
/// @see head
Value tail(const Value &x, size_t k = 8, std::pmr::memory_resource *mr = nullptr);

/// @brief Pads array with zeros or a constant to a specified target length.
/// @param x Input array.
/// @param len Target length along dimension.
/// @param dim Dimension to pad along (0 = first non-singleton).
/// @param side "right" (trailing) or "left" (leading).
/// @param mr Memory resource.
/// @return Padded array.
/// @see trimdata
Value paddata(const Value &x, size_t len, int dim = 0, const std::string &side = "right", std::pmr::memory_resource *mr = nullptr);

/// @brief Trims array to a specified target length along a dimension.
/// @param x Input array.
/// @param len Target length.
/// @param dim Dimension to trim along (0 = first non-singleton).
/// @param side "right" (trailing) or "left" (leading).
/// @param mr Memory resource.
/// @return Trimmed array.
Value trimdata(const Value &x, size_t len, int dim = 0, const std::string &side = "right", std::pmr::memory_resource *mr = nullptr);

/// @brief Applies element-wise binary operator with singleton expansion (broadcasting).
/// @param op Binary operator name (e.g. "plus", "times", "rdivide", "power").
/// @param a First array operand.
/// @param b Second array operand.
/// @param mr Memory resource.
/// @return Broadcasted result array.
Value bsxfun(const std::string &op, const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts lower triangular part of a matrix.
/// @param x Input matrix.
/// @param k Diagonal offset (0=main, >0=above, <0=below).
/// @param mr Memory resource.
/// @return Lower triangular matrix.
/// @see triu
Value tril(const Value &x, int k = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts upper triangular part of a matrix.
/// @param x Input matrix.
/// @param k Diagonal offset (0=main, >0=above, <0=below).
/// @param mr Memory resource.
/// @return Upper triangular matrix.
/// @see tril
Value triu(const Value &x, int k = 0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
