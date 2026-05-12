// libs/builtin/include/numkit/builtin/math/arithmetic/reductions.hpp
//
// Reductions (sum / prod / mean / max / min) and the array generators
// linspace / logspace.
//
// Single-return reductions pick the reduction axis automatically:
// vectors collapse to scalar, 2D matrices reduce along columns (dim=1),
// 3D arrays reduce along the first non-singleton dim — matching MATLAB's
// no-arg default. Three-arg form takes an explicit 1-based dim (passing
// 0 is equivalent to omitting the argument).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::builtin {

Value sum(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value sum(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value prod(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value prod(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
Value mean(const Value &x, std::pmr::memory_resource *mr = nullptr);
Value mean(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

//// Vector input → scalar (value, 1-based idx). Matrix input → column-
//// wise reduction (row vector of values + indices). 3D input →
//// reduction along first non-singleton dim.
std::tuple<Value, Value> max(const Value &x, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> min(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Same with explicit 1-based dim; dim==0 means auto-detect.
std::tuple<Value, Value> max(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value> min(const Value &x, int dim, std::pmr::memory_resource *mr = nullptr);

/// Elementwise max/min of two arrays (with broadcasting).
Value max(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);
Value min(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

//// Equally spaced vector, length n (default 100). Endpoints included.
Value linspace(double a, double b, size_t n = 100, std::pmr::memory_resource *mr = nullptr);

/// Logarithmically-spaced vector: 10^a ... 10^b, length n (default 50).
Value logspace(double a, double b, size_t n = 50, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
