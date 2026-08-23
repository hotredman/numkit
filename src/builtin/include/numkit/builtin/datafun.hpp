// src/builtin/include/numkit/builtin/datafun.hpp
//
// Pure C++ Data analysis, reductions, random number generation, set operations.
#pragma once

#include <memory_resource>
#include <string>
#include <tuple>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/rng_context.hpp>

namespace numkit::builtin {

using ::numkit::ops::RngContext;

/// @file
/// @brief Data analysis, reductions, search, set operations, and random distributions.
///
/// Provides a clean, engine-free C++ API for reduction operators (sum, mean, min, max, prod),
/// pseudo-random number distributions, and set operations.

// ── Reductions ──────────────────────────────────────────────────────────────

Value sum(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value sum(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Product of array elements along specified dimension.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Product array.
/// @see sum, mean
Value prod(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value prod(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Mean (arithmetic average) of array elements.
/// @param x Input array.
/// @param dim Dimension along which to operate (0 = first non-singleton).
/// @param mr Memory resource.
/// @return Mean array.
/// @see sum, median
Value mean(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value mean(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// @brief Largest elements in array or element-wise maximum between two arrays.
#include <tuple>

/// @brief Largest elements in array. Returns tuple `(values, indices)`.
std::tuple<Value, Value> max(const Value &x, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> max(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value max(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> maxOmitNan(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value maxOmitNanBinary(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Smallest elements in array. Returns tuple `(values, indices)`.
std::tuple<Value, Value> min(const Value &x, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> min(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value min(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> minOmitNan(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value minOmitNanBinary(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value cumsum(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cumsum(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value cumprod(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cumprod(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value cummax(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cummax(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value cummin(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value cummin(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value diff(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value diff(const Value &x, int n, int dim = 0, std::pmr::memory_resource *mr = nullptr);

Value anyOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);
Value allOf(const Value &x, int dim = 0, std::pmr::memory_resource *mr = nullptr);

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



enum class HistBinMethod {
    Auto,
    Scott,
    Fd,
    Integers,
    Sturges,
    Sqrt
};

enum class HistNorm {
    Count,
    Probability,
    CountDensity,
    Pdf,
    CumCount,
    Cdf
};

struct HistBinSpec {
    HistBinMethod method = HistBinMethod::Auto;
    bool hasNumBins = false;
    double numBins = 0.0;
    bool hasBinWidth = false;
    double binWidth = 0.0;
    bool hasBinLimits = false;
    double limLo = 0.0;
    double limHi = 0.0;
};

/// @brief Set operations and discrete functions.
Value unique(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false);
Value uniqueRows(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false);
std::tuple<Value, Value, Value> uniqueWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false, bool last = false);
std::tuple<Value, Value, Value> uniqueRowsWithIndices(const Value &x, std::pmr::memory_resource *mr = nullptr, bool stable = false, bool last = false);

Value ismember(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
std::pair<Value, Value> ismemberComplex(const Value &a, const Value &b, bool wantLoc, std::pmr::memory_resource *mr);

Value setUnion(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);
Value setIntersect(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);
Value setDiff(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr, bool stable = false);
Value setxor(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

Value discretize(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);
Value histc(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);
Value histcounts(const Value &x, const Value &edges, std::pmr::memory_resource *mr = nullptr);
Value histcounts(const Value &x, const Value &edges, HistNorm norm, std::pmr::memory_resource *mr = nullptr);
Value histcountsAutoEdges(const Value &x, const HistBinSpec &spec, std::pmr::memory_resource *mr = nullptr);
Value allunique(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value numunique(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value ismembertol(const Value &a, const Value &s, double tol = 1e-6, std::pmr::memory_resource *mr = nullptr);
Value uniquetol(const Value &x, double tol = 1e-6, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
