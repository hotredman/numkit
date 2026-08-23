// include/numkit/builtin/datafun.hpp
//
// Data analysis, reductions, random number generation, set operations.
#pragma once

#include <memory_resource>
#include <string>
#include <tuple>
#include <vector>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @file
/// @brief Data analysis, reductions, search, set operations, and random distributions.

// ── Reductions ──────────────────────────────────────────────────────────────

/// @brief Sum of array elements along specified dimension.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Sum array.
Value sum(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Product of array elements along specified dimension.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Product array.
Value prod(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean (average) of array elements.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Mean array.
Value mean(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest elements in array.
/// @param a First array or input array.
/// @param b Second array for elementwise max (empty if reduction over @p dim).
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Maximum values.
Value max(const Value &a, const Value &b = Value(), int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest elements in array.
/// @param a First array or input array.
/// @param b Second array for elementwise min (empty if reduction over @p dim).
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Minimum values.
Value min(const Value &a, const Value &b = Value(), int dim = 0, std::pmr::memory_resource *mr = nullptr);

// ── Random Number Generation ────────────────────────────────────────────────

/// @brief Uniformly distributed random numbers in [0, 1).
/// @param rows Row count.
/// @param cols Column count.
/// @param mr Memory resource.
/// @return Random matrix.
Value rand(size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Normally distributed random numbers (mean 0, std 1).
/// @param rows Row count.
/// @param cols Column count.
/// @param mr Memory resource.
/// @return Standard normal random matrix.
Value randn(size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Uniformly distributed random integers in [imin, imax].
/// @param imin Minimum integer.
/// @param imax Maximum integer.
/// @param rows Row count.
/// @param cols Column count.
/// @param mr Memory resource.
/// @return Random integer matrix.
Value randi(int imin, int imax, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Random permutation of integers 1 to n.
/// @param n Integer upper bound.
/// @param k Number of elements to pick (0 = all n).
/// @param mr Memory resource.
/// @return Row vector of permuted integers.
Value randperm(size_t n, size_t k = 0, std::pmr::memory_resource *mr = nullptr);

// ── Set Operations ──────────────────────────────────────────────────────────

/// @brief Unique values in array.
/// @param x Input array.
/// @param mr Memory resource.
/// @return Sorted vector of unique values.
Value unique(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Array elements that are members of set.
/// @param a Input array.
/// @param s Set array.
/// @param mr Memory resource.
/// @return Logical array where true indicates membership.
Value ismember(const Value &a, const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Set union of two arrays.
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted union vector.
Value union_set(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Set intersection of two arrays.
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted intersection vector.
Value intersect(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Set difference of two arrays (elements in @p a not in @p b).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted set difference vector.
Value setdiff(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Set exclusive OR of two arrays.
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted XOR vector.
Value setxor(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);



// ── Registration ────────────────────────────────────────────────────────────

/// @brief Registers all data analysis builtins into the engine instance.
void register_datafun(Engine &engine);

} // namespace numkit::builtin
