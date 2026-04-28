// libs/builtin/include/numkit/m/builtin/math/elementary/reductions.hpp
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

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::builtin {

// ── Single-return reductions ─────────────────────────────────────────
MValue sum(Allocator &alloc, const MValue &x);
MValue sum(Allocator &alloc, const MValue &x, int dim);
MValue prod(Allocator &alloc, const MValue &x);
MValue prod(Allocator &alloc, const MValue &x, int dim);
MValue mean(Allocator &alloc, const MValue &x);
MValue mean(Allocator &alloc, const MValue &x, int dim);

// ── max/min — multi-return (value, index) or elementwise binary ──────
/// Vector input → scalar (value, 1-based idx). Matrix input → column-
/// wise reduction (row vector of values + indices). 3D input →
/// reduction along first non-singleton dim.
std::tuple<MValue, MValue> max(Allocator &alloc, const MValue &x);
std::tuple<MValue, MValue> min(Allocator &alloc, const MValue &x);

/// Same with explicit 1-based dim; dim==0 means auto-detect.
std::tuple<MValue, MValue> max(Allocator &alloc, const MValue &x, int dim);
std::tuple<MValue, MValue> min(Allocator &alloc, const MValue &x, int dim);

/// Elementwise max/min of two arrays (with broadcasting).
MValue max(Allocator &alloc, const MValue &a, const MValue &b);
MValue min(Allocator &alloc, const MValue &a, const MValue &b);

// ── Array generators ─────────────────────────────────────────────────
/// Equally spaced vector, length n (default 100). Endpoints included.
MValue linspace(Allocator &alloc, double a, double b, size_t n = 100);

/// Logarithmically-spaced vector: 10^a ... 10^b, length n (default 50).
MValue logspace(Allocator &alloc, double a, double b, size_t n = 50);

} // namespace numkit::m::builtin
