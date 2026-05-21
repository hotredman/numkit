// libs/builtin/include/numkit/builtin/math/random/rng.hpp
//
// RNG management + integer-typed random generators.

#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <numkit/builtin/math/random/matlab_mt19937.hpp>

#include <cstdint>
#include <mutex>
#include <random>

namespace numkit::builtin {

/// @file
/// @brief Random number generation builtins.
///
/// All RNG-using functions (`rand`, `randn`, `randi`, `randperm`) share
/// a single process-static engine. Reproducibility:
/// ```matlab
///   rng(0)            % deterministic seed
///   rng('default')    % the default seed (0)
///   rng('shuffle')    % seed from std::random_device
///   s = rng();        % save state (struct with .Type, .State)
///   rng(s);           % restore state — subsequent calls reproduce
/// ```
/// **Note:** RNG is process-wide, not per-Engine. Multi-engine programs
/// share the sequence. Per-Engine RNG plumbing is a separate refactor.

/// @brief Uniform `[0, 1)` random matrix
/// (`r = rand(rng, rows, cols, pages)`).
///
/// `pages == 0` produces a 2-D matrix.
///
/// @param rng    MT19937 engine reference.
/// @param rows   Output rows.
/// @param cols   Output columns (default 1).
/// @param pages  Output pages (0 → 2-D).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Random DOUBLE matrix.
/// @see randn, randND
Value rand(detail::MatlabMT19937 &rng, size_t rows, size_t cols = 1,
           size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Standard normal random matrix
/// (`r = randn(rng, rows, cols, pages)`).
///
/// @param rng    MT19937 engine reference.
/// @param rows   Output rows.
/// @param cols   Output columns (default 1).
/// @param pages  Output pages (0 → 2-D).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Random DOUBLE matrix from `N(0, 1)`.
/// @see rand, randnND
Value randn(detail::MatlabMT19937 &rng, size_t rows, size_t cols = 1,
            size_t pages = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief ND uniform `[0, 1)` (`r = randND(rng, dims)`).
///
/// @param rng   MT19937 engine reference.
/// @param dims  Output shape (any rank ≥ 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Random DOUBLE array of the given shape.
/// @see rand
Value randND(detail::MatlabMT19937 &rng, Span<const size_t> dims,
             std::pmr::memory_resource *mr = nullptr);

/// @brief ND standard normal (`r = randnND(rng, dims)`).
///
/// @param rng   MT19937 engine reference.
/// @param dims  Output shape (any rank ≥ 1).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Random DOUBLE array from `N(0, 1)`.
/// @see randn
Value randnND(detail::MatlabMT19937 &rng, Span<const size_t> dims,
              std::pmr::memory_resource *mr = nullptr);

// ── Seeding / state control ──────────────────────────────────────────

/// @brief Access the process-wide shared MT19937.
///
/// Use this when composing new samplers (e.g. distribution-specific
/// `*rnd` in `libs/stats`) so all RNG paths share the same state and
/// respect `rng(seed)`. Wrap accesses in
/// `std::lock_guard<std::mutex>{rngMutex()}` if your caller can race
/// with `rand` / `randn` / `randi`.
///
/// @return Mutable reference to the shared engine.
/// @see rngMutex
detail::MatlabMT19937 &sharedEngine();

/// @brief Mutex protecting the shared engine.
///
/// @return Reference to the global RNG mutex.
/// @see sharedEngine
std::mutex &rngMutex();

/// @brief Seed the shared RNG.
///
/// `seed = 0` is equivalent to `rng('default')`.
///
/// @param seed  Seed value.
void rngSeed(uint64_t seed);

/// @brief Seed from `std::random_device` (`rng('shuffle')`).
///
/// Non-reproducible — uses entropy source.
void rngShuffle();

/// @brief Snapshot the current RNG state (`s = rng()`).
///
/// Returns a struct `{.Type = 'twister', .State = …}`. Pass the same
/// struct back to @ref rngRestore to reproduce subsequent calls.
///
/// @param mr  Memory resource (nullptr → process default).
/// @return    State struct.
/// @see rngRestore
Value rngState(std::pmr::memory_resource *mr = nullptr);

/// @brief Restore the shared RNG state (`rng(s)`).
///
/// @param state  Struct previously produced by @ref rngState.
void rngRestore(const Value &state);

/// @brief Uniform random integer in `[1, imax]` (`r = randi(imax)`).
///
/// @param imax  Upper bound (inclusive).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Scalar random integer (DOUBLE-typed).
/// @see randi(imax, rows, cols, pages, mr)
Value randi(int64_t imax, std::pmr::memory_resource *mr = nullptr);

/// @brief Uniform integer matrix in `[1, imax]`
/// (`r = randi(imax, rows, cols, pages)`).
///
/// @param imax   Upper bound (inclusive).
/// @param rows   Output rows.
/// @param cols   Output columns.
/// @param pages  Output pages (0 → 2-D).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Random integer matrix (DOUBLE storage).
Value randi(int64_t imax, size_t rows, size_t cols, size_t pages = 0,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Uniform integer matrix in `[imin, imax]`
/// (`r = randi([imin imax], rows, cols, pages)`).
///
/// @param imin   Lower bound (inclusive).
/// @param imax   Upper bound (inclusive).
/// @param rows   Output rows.
/// @param cols   Output columns.
/// @param pages  Output pages (0 → 2-D).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Random integer matrix.
Value randi(int64_t imin, int64_t imax, size_t rows, size_t cols,
            size_t pages = 0,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Random permutation of `1:n` (`p = randperm(n)`).
///
/// @param n   Permutation size.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row vector containing a permutation of `1..n`.
Value randperm(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief First `k` of a random permutation
/// (`p = randperm(n, k)`).
///
/// `k` unique random integers from `1..n` (`k <= n`).
///
/// @param n   Range upper bound.
/// @param k   Number of unique draws.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `1 × k` row vector.
Value randperm(size_t n, size_t k, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
