// libs/builtin/include/numkit/builtin/math/elementary/reductions.hpp
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

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::builtin {

// ── Single-return reductions ─────────────────────────────────────────
Value sum(Allocator &alloc, const Value &x);
Value sum(Allocator &alloc, const Value &x, int dim);
Value prod(Allocator &alloc, const Value &x);
Value prod(Allocator &alloc, const Value &x, int dim);
Value mean(Allocator &alloc, const Value &x);
Value mean(Allocator &alloc, const Value &x, int dim);

// ── max/min — multi-return (value, index) or elementwise binary ──────
/// Vector input → scalar (value, 1-based idx). Matrix input → column-
/// wise reduction (row vector of values + indices). 3D input →
/// reduction along first non-singleton dim.
std::tuple<Value, Value> max(Allocator &alloc, const Value &x);
std::tuple<Value, Value> min(Allocator &alloc, const Value &x);

/// Same with explicit 1-based dim; dim==0 means auto-detect.
std::tuple<Value, Value> max(Allocator &alloc, const Value &x, int dim);
std::tuple<Value, Value> min(Allocator &alloc, const Value &x, int dim);

/// Elementwise max/min of two arrays (with broadcasting).
Value max(Allocator &alloc, const Value &a, const Value &b);
Value min(Allocator &alloc, const Value &a, const Value &b);

// ── Array generators ─────────────────────────────────────────────────
/// Equally spaced vector, length n (default 100). Endpoints included.
Value linspace(Allocator &alloc, double a, double b, size_t n = 100);

/// Logarithmically-spaced vector: 10^a ... 10^b, length n (default 50).
Value logspace(Allocator &alloc, double a, double b, size_t n = 50);

} // namespace numkit::builtin
