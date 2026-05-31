// libs/builtin/include/numkit/builtin/math/geom/geom.hpp
//
// Geometric / graph-algorithm primitives. Most fns in libs/builtin/src/
// math/geom/geom.cpp are adapter-only (script-callable but no C++ API);
// this header declares the ones with explicit typed entry points.

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// @brief N-D scattered-data interpolation (`vi = griddatan(X, v, xi)`).
///
/// Interpolates the values `v[i]` defined at the m points stored in the
/// rows of `X` (`m × n`) to the k query points in `xi` (`k × n`).
///
/// Method support:
///   - `"nearest"` — brute-force Euclidean nearest-neighbour (any `n`).
///   - `"linear"`  — barycentric over Delaunay (only `n == 2` in v1).
///
/// `"linear"` for `n ≥ 3` is a v1 KNOWN GAP — needs a real N-D Delaunay
/// (Qhull-style), which isn't shipped yet. Calling it raises
/// `m:griddatan:linearNDUnsupported` with a clear pointer.
///
/// @param X       `m × n` data points.
/// @param v       Length-`m` values column.
/// @param xi      `k × n` query points.
/// @param method  `"linear"` (default) or `"nearest"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Length-`k` column of interpolated values.
/// @throws Error on shape mismatch, unknown method, or `'linear'` with
///         `n ≥ 3` (v1 gap).
Value griddatan(const Value &X, const Value &v, const Value &xi,
                const std::string &method = "linear",
                std::pmr::memory_resource *mr = nullptr);

/// @brief Match-pairs result.
struct MatchpairsResult {
    Value M;   ///< `p × 2` matrix of `(row, col)` 1-based match pairs.
    Value uR;  ///< Column of unmatched row indices (1-based).
    Value uC;  ///< Column of unmatched col indices (1-based).
};

/// @brief Linear assignment / bipartite matching
/// (`[M, uR, uC] = matchpairs(Cost, costUnmatched [, mode])`).
///
/// Solves the LAP on a rectangular `Cost` matrix with an unmatched-cost
/// option (per row / per col) using the Jonker-Volgenant Hungarian
/// algorithm on an augmented `(m + n) × (m + n)` cost matrix.
///
/// Mode semantics (matches MATLAB R2025b):
///   - `'min'` (default) — minimise total cost; `costUnmatched` is a
///     PENALTY for leaving a row/col unmatched.
///   - `'max'`           — maximise total benefit; `costUnmatched` is a
///     REWARD for leaving unmatched. A high positive value pushes the
///     solver to leave everything unmatched.
///
/// @param Cost            `m × n` cost (or benefit, in `'max'`) matrix.
///                        Complex Cost is not supported.
/// @param costUnmatched   Per-row / per-col penalty (`'min'`) or reward
///                        (`'max'`) for leaving unmatched.
/// @param mode            `"min"` (default) or `"max"`.
/// @param mr              Memory resource (nullptr → process default).
/// @return                `{ M, uR, uC }` — match pairs + unmatched
///                        row/col index columns (all 1-based).
MatchpairsResult matchpairs(const Value &Cost, double costUnmatched,
                            const std::string &mode = "min",
                            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
