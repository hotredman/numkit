// libs/builtin/include/numkit/builtin/math/random/rng.hpp
//
// RNG management + integer-typed random generators.
//
// Phase-4 of the parity expansion plan. Supersedes the per-function
// `static std::mt19937` previously baked into the math TUs, which made
// it impossible to (a) seed reproducibly from MATLAB code or (b) keep
// `rand` and `randn` on the same sequence as MATLAB does.
//
// All RNG-using functions (rand, randn, randi, randperm) share a
// single process-static engine. Reproducibility:
//
//   rng(0)            % deterministic seed
//   rng('default')    % MATLAB's default seed (0)
//   rng('shuffle')    % seed from std::random_device
//   s = rng();        % save state (struct with .Type, .State)
//   rng(s);           % restore state — subsequent calls reproduce
//
// Note: RNG is process-wide, not per-Engine. Multi-engine programs
// share the sequence — same caveat as the pre-Phase-4 code carried.
// Plumbing per-Engine RNG is a separate refactor (Engine API change).

#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <numkit/builtin/math/random/matlab_mt19937.hpp>

#include <cstdint>
#include <mutex>
#include <random>

namespace numkit::builtin {

//// Uniform [0, 1) random matrix. rows/cols/pages == 0 for pages means 2D.
Value rand(detail::MatlabMT19937 &rng, size_t rows, size_t cols = 1, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// Standard normal random matrix.
Value randn(detail::MatlabMT19937 &rng, size_t rows, size_t cols = 1, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// ND uniform [0, 1) — accepts any rank ≥ 1.
Value randND(detail::MatlabMT19937 &rng, Span<const size_t> dims, std::pmr::memory_resource *mr = nullptr);

/// ND standard normal — accepts any rank ≥ 1.
Value randnND(detail::MatlabMT19937 &rng, Span<const size_t> dims, std::pmr::memory_resource *mr = nullptr);

// ── Seeding / state control ──────────────────────────────────────────

/// The process-wide shared mt19937 engine. Use this when composing
/// new random samplers (e.g. distribution-specific *rnd in libs/stats)
/// so all RNG paths share the same state and respect rng(seed).
/// Wrap accesses in `std::lock_guard<std::mutex>{rngMutex()}` if your
/// caller can race with rand / randn / randi.
detail::MatlabMT19937 &sharedEngine();
std::mutex &rngMutex();

/// Seed the shared RNG. seed=0 matches MATLAB's rng('default').
void rngSeed(uint64_t seed);

/// Seed from std::random_device (non-reproducible). MATLAB rng('shuffle').
void rngShuffle();

/// Snapshot the current state into a struct {.Type='twister', .State=…}.
/// Pass the same struct back to rngRestore to reproduce subsequent calls.
Value rngState(std::pmr::memory_resource *mr = nullptr);

/// Restore from a previously-snapshotted state struct.
void rngRestore(const Value &state);

//// randi(imax) — uniform integer in [1, imax].
Value randi(int64_t imax, std::pmr::memory_resource *mr = nullptr);

/// randi(imax, rows, cols, pages) — array of ints in [1, imax].
Value randi(int64_t imax, size_t rows, size_t cols, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// randi([imin imax], …) — uniform integer in [imin, imax].
Value randi(int64_t imin, int64_t imax, size_t rows, size_t cols, size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

//// randperm(n) — random permutation of 1:n.
Value randperm(size_t n, std::pmr::memory_resource *mr = nullptr);

/// randperm(n, k) — k unique random ints from 1:n (k <= n).
Value randperm(size_t n, size_t k, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
