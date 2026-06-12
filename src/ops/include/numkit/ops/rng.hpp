// ops/include/numkit/ops/rng.hpp
//
// Value-producing random generators (rand / randn / randi / randperm). Every
// one draws from a caller-provided RngContext — the seedable session stream the
// Engine owns (engine.rng()); there is NO process-global RNG and NO mutex. A
// single session shares one reproducible stream (MATLAB `rng(seed)` semantics);
// two Engines are independent. RngContext models UniformRandomBitGenerator, so
// the std distributions here draw from it directly (`dist(rng)`).
//
// An L0.5 ops primitive (Value + RngContext, no Engine). The user-facing
// rand/randn/randi/randperm/rng *builtins* (the CallContext wrappers that parse
// shape/type args) live in bundle and call these with engine.rng().

#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <numkit/ops/rng_context.hpp>

#include <cstdint>

namespace numkit::ops {

// ── Real-valued random (uniform / standard-normal) ───────────────────
// `pages == 0` → 2-D matrix. Output is DOUBLE.
Value rand(RngContext &rng, size_t rows, size_t cols = 1,
           size_t pages = 0, std::pmr::memory_resource *mr = nullptr);
Value randn(RngContext &rng, size_t rows, size_t cols = 1,
            size_t pages = 0, std::pmr::memory_resource *mr = nullptr);
Value randND(RngContext &rng, Span<const size_t> dims,
             std::pmr::memory_resource *mr = nullptr);
Value randnND(RngContext &rng, Span<const size_t> dims,
              std::pmr::memory_resource *mr = nullptr);

// ── Integer random ───────────────────────────────────────────────────
Value randi(RngContext &rng, int64_t imax, std::pmr::memory_resource *mr = nullptr);
Value randi(RngContext &rng, int64_t imax, size_t rows, size_t cols,
            size_t pages = 0, std::pmr::memory_resource *mr = nullptr);
Value randi(RngContext &rng, int64_t imin, int64_t imax, size_t rows, size_t cols,
            size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

// ── Permutations ─────────────────────────────────────────────────────
Value randperm(RngContext &rng, size_t n, std::pmr::memory_resource *mr = nullptr);
Value randperm(RngContext &rng, size_t n, size_t k,
               std::pmr::memory_resource *mr = nullptr);

// rng() control (seed / shuffle / state save+restore) lives on RngContext
// itself — see rng_context.hpp.

} // namespace numkit::ops
