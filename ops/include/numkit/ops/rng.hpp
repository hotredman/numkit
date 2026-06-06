// ops/include/numkit/ops/rng.hpp
//
// Shared RNG compute: the process-wide MT19937 stream + the value-producing
// generators (rand/randn/randi/randperm) and rng() state control. An L0.5 ops
// primitive (Value + MatlabMT19937 only, no engine) so any toolbox that needs
// randomness (comm noise, stats *rnd samplers, …) shares one seedable stream
// without depending on builtin/the engine.
//
// The user-facing rand/randn/randi/randperm/rng *builtins* (the CallContext
// wrappers that parse shape/type args) stay in libs/builtin and call these.

#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <numkit/ops/matlab_mt19937.hpp>

#include <cstdint>
#include <mutex>
#include <random>

namespace numkit::ops {

// ── Real-valued random (engine passed explicitly) ────────────────────
// `pages == 0` → 2-D matrix. Output is DOUBLE.
Value rand(MatlabMT19937 &rng, size_t rows, size_t cols = 1,
           size_t pages = 0, std::pmr::memory_resource *mr = nullptr);
Value randn(MatlabMT19937 &rng, size_t rows, size_t cols = 1,
            size_t pages = 0, std::pmr::memory_resource *mr = nullptr);
Value randND(MatlabMT19937 &rng, Span<const size_t> dims,
             std::pmr::memory_resource *mr = nullptr);
Value randnND(MatlabMT19937 &rng, Span<const size_t> dims,
              std::pmr::memory_resource *mr = nullptr);

// ── Shared process-wide engine + its mutex ───────────────────────────
// Compose new samplers against sharedEngine() so all RNG paths share state
// and respect rng(seed). Guard with std::lock_guard{rngMutex()} if racy.
MatlabMT19937 &sharedEngine();
std::mutex &rngMutex();

// ── Seeding / state control (operate on the shared engine) ───────────
void rngSeed(uint64_t seed);                                  // seed=0 ≡ rng('default')
void rngShuffle();                                            // rng('shuffle')
Value rngState(std::pmr::memory_resource *mr = nullptr);      // s = rng()
void rngRestore(const Value &state);                          // rng(s)

// ── Integer random (use the shared engine) ───────────────────────────
Value randi(int64_t imax, std::pmr::memory_resource *mr = nullptr);
Value randi(int64_t imax, size_t rows, size_t cols, size_t pages = 0,
            std::pmr::memory_resource *mr = nullptr);
Value randi(int64_t imin, int64_t imax, size_t rows, size_t cols,
            size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

// ── Permutations (use the shared engine) ─────────────────────────────
Value randperm(size_t n, std::pmr::memory_resource *mr = nullptr);
Value randperm(size_t n, size_t k, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::ops
