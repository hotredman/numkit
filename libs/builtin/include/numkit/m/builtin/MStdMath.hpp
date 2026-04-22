// libs/builtin/include/numkit/m/builtin/MStdMath.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <random>
#include <tuple>

namespace numkit::m::builtin {

// ── Elementwise unary — double with complex promotion ────────────────
//
// `hint` (when non-null and a uniquely-owned heap double of matching
// shape) is reused as the result buffer instead of allocating a fresh
// one. The function moves out of `*hint` into the returned MValue —
// after the call `*hint` is empty. Saves the per-call N-element alloc
// at large N where Windows HeapAlloc spills into VirtualAlloc / page
// commit (see BM_PlusAlloc / BM_PlusKernel benches). When hint is
// nullptr or doesn't match (different shape, complex, scalar input,
// shared ownership) the function falls back to the standard alloc
// path and `*hint` is left untouched.
//
// Only the SIMD-backed unaries (abs/sin/cos/exp/log) honour the hint;
// the others ignore it for now (they go through the slower scalar
// elementwiseDouble helper anyway, where the alloc cost matters less).
MValue sqrt(Allocator &alloc, const MValue &x);
MValue abs(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue sin(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue cos(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue tan(Allocator &alloc, const MValue &x);
MValue asin(Allocator &alloc, const MValue &x);
MValue acos(Allocator &alloc, const MValue &x);
MValue atan(Allocator &alloc, const MValue &x);
MValue exp(Allocator &alloc, const MValue &x, MValue *hint = nullptr);
MValue log(Allocator &alloc, const MValue &x, MValue *hint = nullptr);

// ── Elementwise unary — double only ───────────────────────────────────
MValue log2(Allocator &alloc, const MValue &x);
MValue log10(Allocator &alloc, const MValue &x);
MValue floor(Allocator &alloc, const MValue &x);
MValue ceil(Allocator &alloc, const MValue &x);
MValue round(Allocator &alloc, const MValue &x);
MValue fix(Allocator &alloc, const MValue &x);          // truncate toward zero
MValue sign(Allocator &alloc, const MValue &x);
MValue deg2rad(Allocator &alloc, const MValue &x);
MValue rad2deg(Allocator &alloc, const MValue &x);

// ── Elementwise binary ───────────────────────────────────────────────
MValue atan2(Allocator &alloc, const MValue &y, const MValue &x);
MValue mod(Allocator &alloc, const MValue &a, const MValue &b);
MValue rem(Allocator &alloc, const MValue &a, const MValue &b);

/// hypot(x, y) — sqrt(x^2 + y^2) without intermediate overflow.
MValue hypot(Allocator &alloc, const MValue &x, const MValue &y);

/// nthroot(x, n) — real n-th root. Negative x with odd n produces a
/// negative real (unlike `x .^ (1/n)` which goes complex).
MValue nthroot(Allocator &alloc, const MValue &x, const MValue &n);

/// expm1(x) — exp(x) - 1, accurate near zero.
MValue expm1(Allocator &alloc, const MValue &x);

/// log1p(x) — log(1 + x), accurate near zero.
MValue log1p(Allocator &alloc, const MValue &x);

// ── Reductions (single-return) ───────────────────────────────────────
//
// Two-arg form picks the reduction axis automatically: vectors collapse
// to a scalar, 2D matrices reduce along columns (dim=1), 3D arrays
// reduce along the first non-singleton dim — matching MATLAB's no-arg
// default. Three-arg form takes an explicit 1-based dim (passing 0 is
// equivalent to omitting the argument).
MValue sum(Allocator &alloc, const MValue &x);
MValue sum(Allocator &alloc, const MValue &x, int dim);
MValue prod(Allocator &alloc, const MValue &x);
MValue prod(Allocator &alloc, const MValue &x, int dim);
MValue mean(Allocator &alloc, const MValue &x);
MValue mean(Allocator &alloc, const MValue &x, int dim);

// ── max/min — multi-return with index, or elementwise binary form ────
/// Return (value, index) reduction. Vector input → scalar (value, 1-based idx).
/// Matrix input → column-wise reduction (row vector of values + indices).
/// 3D input → reduction along first non-singleton dimension.
std::tuple<MValue, MValue> max(Allocator &alloc, const MValue &x);
std::tuple<MValue, MValue> min(Allocator &alloc, const MValue &x);

/// Same as above but along an explicit 1-based dim (MATLAB's
/// max(X, [], dim) / min(X, [], dim) form). dim==0 means auto-detect.
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

/// Uniform [0, 1) random matrix. rows/cols/pages == 0 for pages means 2D.
MValue rand(Allocator &alloc,
            std::mt19937 &rng,
            size_t rows,
            size_t cols = 1,
            size_t pages = 0);

/// Standard normal random matrix.
MValue randn(Allocator &alloc,
             std::mt19937 &rng,
             size_t rows,
             size_t cols = 1,
             size_t pages = 0);

} // namespace numkit::m::builtin
