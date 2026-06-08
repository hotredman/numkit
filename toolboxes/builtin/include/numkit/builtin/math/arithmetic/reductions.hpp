// toolboxes/builtin/include/numkit/builtin/math/arithmetic/reductions.hpp
//
// Reductions (sum / prod / mean / max / min) and the array generators
// linspace / logspace.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>

namespace numkit::builtin {

/// @file
/// @brief Reductions and equally-spaced array generators.
///
/// **Dim convention** (auto-dim form): vectors collapse to scalar, 2-D
/// matrices reduce along columns (dim=1), 3-D arrays reduce along the
/// first non-singleton dim (the no-arg default). The
/// three-arg form takes an explicit 1-based `dim` (passing 0 is
/// equivalent to omitting the argument).

/// @brief Sum, auto-dim (`y = sum(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Reduced sum.
/// @see sum(x, dim, mr), prod, mean
Value sum(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Sum along `dim` (`y = sum(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension (0 → auto).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Reduced sum along `dim`.
Value sum(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Product, auto-dim (`y = prod(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Reduced product.
/// @see prod(x, dim, mr), sum
Value prod(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Product along `dim` (`y = prod(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension (0 → auto).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Reduced product along `dim`.
Value prod(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean, auto-dim (`y = mean(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Reduced arithmetic mean.
/// @see mean(x, dim, mr), sum
Value mean(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean along `dim` (`y = mean(x, dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension (0 → auto).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Reduced mean along `dim`.
Value mean(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Maximum + index, auto-dim (`[v, i] = max(x)`).
///
/// Vector input → scalar `(value, 1-based idx)`. Matrix input →
/// column-wise reduction (row vector of values + indices). 3-D input →
/// reduction along first non-singleton dim.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(value, index)` pair.
/// @see max(x, dim, mr), max(a, b, mr), min
std::tuple<Value, Value> max(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Minimum + index, auto-dim (`[v, i] = min(x)`).
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(value, index)` pair.
/// @see min(x, dim, mr), min(a, b, mr), max
std::tuple<Value, Value> min(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Maximum along `dim` (`[v, i] = max(x, [], dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension (0 → auto).
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(value, index)` pair along `dim`.
std::tuple<Value, Value> max(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Minimum along `dim` (`[v, i] = min(x, [], dim)`).
///
/// @param x    Input array.
/// @param dim  1-based dimension (0 → auto).
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(value, index)` pair along `dim`.
std::tuple<Value, Value> min(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise max of two arrays (`y = max(a, b)`).
///
/// Broadcasts elementwise.
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise maximum, broadcast shape.
/// @see min(a, b, mr)
Value max(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise min of two arrays (`y = min(a, b)`).
///
/// @param a   First operand.
/// @param b   Second operand.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Elementwise minimum, broadcast shape.
Value min(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Equally-spaced vector (`v = linspace(a, b, n)`).
///
/// Endpoints `a` and `b` are included.
///
/// @param a   Start value.
/// @param b   End value.
/// @param n   Sample count (default 100).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of length `n`.
/// @see logspace
Value linspace(double a, double b, size_t n = 100, std::pmr::memory_resource *mr = nullptr);

/// @brief Logarithmically-spaced vector (`v = logspace(a, b, n)`).
///
/// Returns `10^a, …, 10^b` (n points, endpoints included).
///
/// @param a   Lower exponent.
/// @param b   Upper exponent.
/// @param n   Sample count (default 50).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector of length `n`.
/// @see linspace
Value logspace(double a, double b, size_t n = 50, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
