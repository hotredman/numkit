// src/builtin/include/numkit/builtin/datafun.hpp
//
// Pure C++ Data analysis, reductions, random number generation, set operations.
#pragma once

#include <memory_resource>
#include <string>
#include <tuple>
#include <vector>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @brief Data analysis, reductions, search, set operations, and random distributions.
///
/// Provides a clean, engine-free C++ API for reduction operators (sum, mean, min, max, prod),
/// pseudo-random number distributions, and set operations.

// ── Reductions ──────────────────────────────────────────────────────────────

/// @brief Sum of array elements along specified dimension.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Sum array.
/// @see prod, mean
Value sum(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Product of array elements along specified dimension.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Product array.
/// @see sum, mean
Value prod(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean (arithmetic average) of array elements.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Mean array.
/// @see sum, median
Value mean(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest elements in array or element-wise maximum between two arrays.
/// @param a First array or input array.
/// @param b Second array for elementwise max (empty if reduction over @p dim).
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Maximum values.
/// @see min
Value max(const Value &a, const Value &b = Value(), int dim = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest elements in array or element-wise minimum between two arrays.
/// @param a First array or input array.
/// @param b Second array for elementwise min (empty if reduction over @p dim).
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Minimum values.
/// @see max
Value min(const Value &a, const Value &b = Value(), int dim = 0, std::pmr::memory_resource *mr = nullptr);

// ── Random Number Generation ────────────────────────────────────────────────

/// @brief Uniformly distributed random numbers in [0, 1).
/// @param rows Row count (default: 1).
/// @param cols Column count (default: 1).
/// @param mr Memory resource.
/// @return `rows x cols` matrix of uniformly distributed random doubles.
/// @see randn, randi, randperm
Value rand(size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Normally distributed random numbers (mean 0, variance 1).
/// @param rows Row count (default: 1).
/// @param cols Column count (default: 1).
/// @param mr Memory resource.
/// @return `rows x cols` matrix of standard normal random doubles.
/// @see rand, randi
Value randn(size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Uniformly distributed random integers in range [imin, imax].
/// @param imin Minimum integer bound.
/// @param imax Maximum integer bound.
/// @param rows Row count (default: 1).
/// @param cols Column count (default: 1).
/// @param mr Memory resource.
/// @return `rows x cols` matrix of random integers.
/// @see rand, randperm
Value randi(int imin, int imax, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);

/// @brief Random permutation of integers 1 to n.
/// @param n Integer upper bound.
/// @param k Number of unique elements to sample (0 = all @p n elements).
/// @param mr Memory resource.
/// @return Row vector of permuted integers.
/// @see rand, randi
Value randperm(size_t n, size_t k = 0, std::pmr::memory_resource *mr = nullptr);

// ── Set Operations ──────────────────────────────────────────────────────────

/// @brief Finds unique values in array.
/// @param x Input array.
/// @param mr Memory resource.
/// @return Sorted vector of unique values.
/// @see ismember, union_set, intersect, setdiff
Value unique(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks which elements of @p a are members of set @p s.
/// @param a Input array to test.
/// @param s Set array.
/// @param mr Memory resource.
/// @return Logical array where true indicates membership in @p s.
/// @see unique, intersect
Value ismember(const Value &a, const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Set union of two arrays (`A ∪ B`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted union vector without duplicates.
/// @see intersect, setdiff, setxor
Value union_set(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Set intersection of two arrays (`A ∩ B`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted intersection vector without duplicates.
/// @see union_set, setdiff, setxor
Value intersect(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Set difference of two arrays (`A \ B`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted vector of elements in @p a that are not in @p b.
/// @see union_set, intersect, setxor
Value setdiff(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Set exclusive OR of two arrays (`(A \ B) ∪ (B \ A)`).
/// @param a First array.
/// @param b Second array.
/// @param mr Memory resource.
/// @return Sorted XOR vector.
/// @see union_set, intersect, setdiff
Value setxor(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
