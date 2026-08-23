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
/// @ingroup group_elmat
/// @brief Elementary matrices and array manipulation functions (MATLAB parity).
///
/// Provides a clean, engine-free C++ API for generating elementary matrices,
/// reshaping, reordering, padding, splitting, and multidimensional array manipulations.

// ── Multi-value Return Types ───────────────────────────────────────────────

/// @brief Result of 2D grid generation via meshgrid/ndgrid.
using Meshgrid2D = std::tuple<Value, Value>;

/// @brief Result of 3D grid generation via meshgrid/ndgrid.
using Meshgrid3D = std::tuple<Value, Value, Value>;

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

/// @brief Creates an identity matrix of size `n x n` (`eye(n)`).
/// @param n Order of the identity matrix.
/// @param mr Memory resource.
/// @return `n x n` identity matrix.
Value eye(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an identity matrix of size `m x n` (`eye(m, n)`).
/// @param m Row count.
/// @param n Column count.
/// @param mr Memory resource.
/// @return `m x n` identity matrix.
Value eye(size_t m, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an identity matrix of size `n x n` with specified data type (`eye(n, dtype)`).
/// @param n Order of the identity matrix.
/// @param dtype Data type (e.g. ValueType::SINGLE, ValueType::INT32).
/// @param mr Memory resource.
/// @return `n x n` typed identity matrix.
Value eye(size_t n, ValueType dtype, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates an identity matrix of size `m x n` with specified data type (`eye(m, n, dtype)`).
/// @param m Row count.
/// @param n Column count.
/// @param dtype Data type.
/// @param mr Memory resource.
/// @return `m x n` typed identity matrix.
Value eye(size_t m, size_t n, ValueType dtype, std::pmr::memory_resource *mr = nullptr);

/// @brief Returns array dimensions as a row vector (`size(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return `1 x D` vector of dimension lengths.
Value size(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Returns length of specified dimension (`size(x, dim)`).
/// @param x Input array.
/// @param dim 1-based dimension index.
/// @param mr Memory resource.
/// @return Dimension length scalar.
Value size(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Length of largest array dimension (`length(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Largest dimension length.
Value length(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Number of array elements (`numel(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Total number of elements.
Value numel(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Number of array dimensions (`ndims(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Dimension count (>= 2).
Value ndims(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise transpose of N-D array (`pagetranspose(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Array with first two dimensions transposed per page.
Value pagetranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Page-wise complex conjugate transpose of N-D array (`pagectranspose(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Array with first two dimensions conjugate transposed per page.
Value pagectranspose(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates peaks example surface function matrix (`peaks(n)`).
/// @param n Order of the square matrix (default: 49).
/// @param mr Memory resource.
/// @return `n x n` peaks matrix.
Value peaks(size_t n = 49, std::pmr::memory_resource *mr = nullptr);

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
/// @param mr Memory resource.
/// @return `n x n` symmetric Pascal matrix.
Value pascal(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates a Pascal matrix with form selector `k` (`pascal(n, k)`).
/// @param n Order of the matrix.
/// @param k Form selector (0=symmetric, 1=lower triangular, 2=upper triangular Cholesky factor).
/// @param mr Memory resource.
/// @return `n x n` Pascal matrix.
Value pascal(size_t n, int k, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Creates companion matrix of polynomial coefficients (`compan(p)`).
/// @param p Polynomial coefficients vector.
/// @param mr Memory resource.
/// @return Companion matrix.
Value compan(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D surface mesh grid coordinate triple.
struct Surface3 {
    Value X;  ///< X coordinates
    Value Y;  ///< Y coordinates
    Value Z;  ///< Z coordinates
};

/// @brief Generates 3-D unit sphere surface coordinates (`[X,Y,Z] = sphere(n)`).
/// @param n Number of facets (default: 20).
/// @param mr Memory resource.
/// @return Surface3 struct.
Surface3 sphere(size_t n = 20, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates 3-D cylinder surface coordinates (`[X,Y,Z] = cylinder(R, n)`).
/// @param R Profile curve radius vector.
/// @param n Number of points around circumference (default: 20).
/// @param mr Memory resource.
/// @return Surface3 struct.
Surface3 cylinder(const Value &R, size_t n = 20, std::pmr::memory_resource *mr = nullptr);

/// @brief Generates 3-D ellipsoid surface coordinates (`[X,Y,Z] = ellipsoid(xc,yc,zc,xr,yr,zr,n)`).
/// @param xc Center x-coordinate.
/// @param yc Center y-coordinate.
/// @param zc Center z-coordinate.
/// @param xr Semi-axis length x.
/// @param yr Semi-axis length y.
/// @param zr Semi-axis length z.
/// @param n Number of facets (default: 20).
/// @param mr Memory resource.
/// @return Surface3 struct.
Surface3 ellipsoid(double xc, double yc, double zc, double xr, double yr, double zr, size_t n = 20, std::pmr::memory_resource *mr = nullptr);

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
Value repmat(const Value &x, size_t r, size_t c, std::pmr::memory_resource *mr = nullptr);

/// @brief 3D convenience overload for repmat (`repmat(x, r, c, p)`).
/// @param x Input array.
/// @param r Row repetition factor.
/// @param c Column repetition factor.
/// @param p Page repetition factor.
/// @param mr Memory resource.
/// @return 3-D tiled array.
Value repmat(const Value &x, size_t r, size_t c, size_t p, std::pmr::memory_resource *mr = nullptr);

/// @brief N-D array tiling overload for repmat.
/// @param x Input array.
/// @param tiles Span of repetition counts.
/// @param mr Memory resource.
/// @return Tiled array.
Value repmatND(const Value &x, Span<const size_t> tiles, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Reshapes array with explicit rows, cols, and optional pages.
/// @param x Input array.
/// @param rows Target row count.
/// @param cols Target column count.
/// @param pages Target page count (0 for 2-D).
/// @param mr Memory resource.
/// @return Reshaped array.
Value reshape(const Value &x, size_t rows, size_t cols, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Circularly shifts elements of 1-D vector or along first non-singleton dimension (`circshift(x, k)`).
/// @param x Input array.
/// @param k Shift amount.
/// @param mr Memory resource.
/// @return Circularly shifted array.
Value circshift(const Value &x, int64_t k, std::pmr::memory_resource *mr = nullptr);

/// @brief Circularly shifts elements of 2-D matrix along rows and columns (`circshift(x, [kRow, kCol])`).
/// @param x Input matrix.
/// @param kRow Row shift amount.
/// @param kCol Column shift amount.
/// @param mr Memory resource.
/// @return Circularly shifted matrix.
Value circshift(const Value &x, int64_t kRow, int64_t kCol, std::pmr::memory_resource *mr = nullptr);

/// @brief Circularly shifts elements of array by shift vector (`circshift(x, shifts)`).
/// @param x Input array.
/// @param shifts Span of shift amounts per dimension.
/// @param mr Memory resource.
/// @return Circularly shifted array.
Value circshift(const Value &x, Span<const int64_t> shifts, std::pmr::memory_resource *mr = nullptr);

/// @brief Circularly shifts elements of N-D array.
/// @param x Input array.
/// @param shifts Span of shift amounts.
/// @param mr Memory resource.
/// @return Circularly shifted array.
Value circshiftND(const Value &x, Span<const int64_t> shifts, std::pmr::memory_resource *mr = nullptr);

/// @brief Repeats elements of array uniformly (`repelem(x, n)`).
/// @param x Input array.
/// @param n Repetition count per element.
/// @param mr Memory resource.
/// @return Array with repeated elements.
Value repelem(const Value &x, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Repeats elements along rows and columns (`repelem(x, m, n)`).
/// @param x Input matrix.
/// @param m Row repetition factor.
/// @param n Column repetition factor.
/// @param mr Memory resource.
/// @return Expanded matrix.
Value repelem(const Value &x, size_t m, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Repeats elements with variable counts per element (`repelem(x, counts)`).
/// @param x Input vector.
/// @param counts Repetition counts vector.
/// @param mr Memory resource.
/// @return Expanded vector.
Value repelem(const Value &x, const Value &counts, std::pmr::memory_resource *mr = nullptr);

/// @brief Repeats elements along rows and columns with separate count vectors (`repelem(x, rCounts, cCounts)`).
/// @param x Input matrix.
/// @param rCounts Row repeat counts vector.
/// @param cCounts Column repeat counts vector.
/// @param mr Memory resource.
/// @return Expanded matrix.
Value repelem(const Value &x, const Value &rCounts, const Value &cCounts, std::pmr::memory_resource *mr = nullptr);

/// @brief Permutes dimensions of an N-D array (`permute(x, order)`).
/// @param x Input array.
/// @param order Dimension permutation order vector (1-based).
/// @param mr Memory resource.
/// @return Permuted array.
/// @see ipermute
Value permute(const Value &x, Span<const int> order, std::pmr::memory_resource *mr = nullptr);

/// @brief Permutes dimensions of an N-D array using size_t order vector.
/// @param x Input array.
/// @param order 1-based dimension indices.
/// @param mr Memory resource.
/// @return Permuted array.
Value permute(const Value &x, Span<const size_t> order, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse permutes dimensions of an N-D array.
/// @param x Input array.
/// @param order Ordering of dimensions used in previous `permute` call.
/// @param mr Memory resource.
/// @return Array with dimensions restored.
Value ipermute(const Value &x, Span<const int> order, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse permutes dimensions of an N-D array using size_t order vector.
/// @param x Input array.
/// @param order Ordering of dimensions.
/// @param mr Memory resource.
/// @return Array with dimensions restored.
Value ipermute(const Value &x, Span<const size_t> order, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of automatic dimension shift (`shiftdim(x)`).
struct ShiftDimAuto {
    Value v;          ///< Shifted array
    int dropped = 0;  ///< Number of leading singleton dimensions removed
};

/// @brief Shifts dimensions of array by `n` steps (`shiftdim(x, n)`).
/// @param x Input array.
/// @param n Number of shifts (positive shifts left, negative shifts right / adds singletons).
/// @param mr Memory resource.
/// @return Shifted array.
/// @see shiftdimAuto, squeeze
Value shiftdim(const Value &x, int n = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Automatically shifts out leading singleton dimensions (`shiftdim(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return ShiftDimAuto struct with shifted array and number of dropped dimensions.
ShiftDimAuto shiftdimAuto(const Value &x, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Pads data array to specified length with zeros (`paddata(x, len)`).
/// @param x Input array.
/// @param len Target length.
/// @param mr Memory resource.
/// @return Padded array.
/// @see trimdata
Value paddata(const Value &x, size_t len, std::pmr::memory_resource *mr = nullptr);

/// @brief Pads data array to length along dimension (`paddata(x, len, dim, side)`).
/// @param x Input array.
/// @param len Target length.
/// @param dim Operating dimension.
/// @param side Direction (`"left"`, `"right"`).
/// @param mr Memory resource.
/// @return Padded array.
Value paddata(const Value &x, size_t len, int dim, const std::string &side = "right", std::pmr::memory_resource *mr = nullptr);

/// @brief Trims data array to specified length (`trimdata(x, len)`).
/// @param x Input array.
/// @param len Target length.
/// @param mr Memory resource.
/// @return Trimmed array.
/// @see paddata
Value trimdata(const Value &x, size_t len, std::pmr::memory_resource *mr = nullptr);

/// @brief Trims data array along dimension (`trimdata(x, len, dim, side)`).
/// @param x Input array.
/// @param len Target length.
/// @param dim Operating dimension.
/// @param side Direction (`"left"`, `"right"`).
/// @param mr Memory resource.
/// @return Trimmed array.
Value trimdata(const Value &x, size_t len, int dim, const std::string &side = "right", std::pmr::memory_resource *mr = nullptr);

/// @brief Resizes 1-D vector to length `n` by padding or trimming (`resize(v, n)`).
/// @param v Input vector.
/// @param n Target length.
/// @param mr Memory resource.
/// @return Resized vector.
Value resize(const Value &v, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts subscript indices to linear index (`sub2ind(siz, i, j, ...)`).
/// @param siz Array size vector.
/// @param subs Subscript index values.
/// @param mr Memory resource.
/// @return Linear index array.
/// @see ind2sub
Value sub2ind(const Value &siz, Span<const Value> subs, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts linear indices to subscript indices (`[I1, I2, ...] = ind2sub(siz, ind)`).
/// @param siz Array size vector.
/// @param ind Linear index array.
/// @param nout Number of subscript outputs requested.
/// @param mr Memory resource.
/// @return Vector of subscript arrays.
/// @see sub2ind
std::vector<Value> ind2sub(const Value &siz, const Value &ind, size_t nout = 0, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Placement policy for NaN elements during sorting.
enum class NanPlace {
    Auto,   ///< Put NaNs at the end
    First,  ///< Put NaNs at the beginning
    Last    ///< Put NaNs at the end
};

/// @brief Sorts array elements in ascending or descending order (`[B, I] = sort(x, dim, direction)`).
/// @param x Input array.
/// @param dim Operating dimension (0 for first non-singleton).
/// @param descend True for descending order.
/// @param nanPlace NaN positioning rule.
/// @param mr Memory resource.
/// @return Tuple containing `{sorted_values, sort_indices}`.
/// @see sortrows, issorted
std::tuple<Value, Value> sort(const Value &x, int dim = 0, bool descend = false, NanPlace nanPlace = NanPlace::Auto, std::pmr::memory_resource *mr = nullptr);

/// @brief Sorts rows of 2-D matrix in ascending order (`[B, I] = sortrows(x)`).
/// @param x Input 2-D matrix.
/// @param mr Memory resource.
/// @return Tuple containing `{sorted_matrix, sort_indices}`.
/// @see sort
std::tuple<Value, Value> sortrows(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Sorts rows of 2-D matrix based on specified columns (`[B, I] = sortrows(x, cols)`).
/// @param x Input 2-D matrix.
/// @param cols 1-based column indices.
/// @param mr Memory resource.
/// @return Tuple containing `{sorted_matrix, sort_indices}`.
std::tuple<Value, Value> sortrows(const Value &x, Span<const int> cols, std::pmr::memory_resource *mr = nullptr);

/// @brief Sorts rows based on columns and directions (`[B, I] = sortrows(x, cols, direction)`).
/// @param x Input 2-D matrix.
/// @param cols Column indices.
/// @param desc Descending flags per column.
/// @param mr Memory resource.
/// @return Tuple containing `{sorted_matrix, sort_indices}`.
std::tuple<Value, Value> sortrows(const Value &x, Span<const int> cols, Span<const bool> desc, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if array is sorted (`issorted(x, dim, direction)`).
/// @param x Input array.
/// @param dim Operating dimension.
/// @param descend True if checking for descending order.
/// @param mr Memory resource.
/// @return Logical true if array is sorted.
Value issorted(const Value &x, int dim = 0, bool descend = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if rows of 2-D matrix are sorted in ascending order (`issortedrows(x)`).
/// @param x Input matrix.
/// @param mr Memory resource.
/// @return Logical true if rows are sorted.
Value issortedrows(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if rows of 2-D matrix are sorted based on columns and directions (`issortedrows(x, cols, desc)`).
/// @param x Input matrix.
/// @param cols Column indices.
/// @param desc Descending flags.
/// @param mr Memory resource.
/// @return Logical true if rows are sorted.
Value issortedrows(const Value &x, Span<const int> cols, Span<const bool> desc = {}, std::pmr::memory_resource *mr = nullptr);

/// @brief Finds the top `k` sorted rows of a matrix (`[B, I] = topkrows(x, k, cols, desc)`).
/// @param x Input matrix.
/// @param k Number of rows to return.
/// @param cols Column indices.
/// @param desc Descending flags.
/// @param mr Memory resource.
/// @return Tuple containing `{top_k_rows, indices}`.
std::tuple<Value, Value> topkrows(const Value &x, size_t k, Span<const int> cols = {}, Span<const bool> desc = {}, std::pmr::memory_resource *mr = nullptr);

/// @brief Finds indices of non-zero array elements (`find(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return 1-based linear indices of non-zero elements.
/// @see nnz, nonzeros
Value find(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Number of non-zero matrix elements (`nnz(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Count of non-zero elements.
/// @see nonzeros, find
Value nnz(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Non-zero elements of matrix as column vector (`nonzeros(x)`).
/// @param x Input array.
/// @param mr Memory resource.
/// @return Column vector of non-zero values.
/// @see nnz, find
Value nonzeros(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Logical exclusive OR of two arrays (`xor(a, b)`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Logical array where true indicates either `a` or `b` is nonzero, but not both.
Value xorOf(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
