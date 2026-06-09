// toolboxes/builtin/include/numkit/builtin/math/geom/geom.hpp
//
// Geometric / graph-algorithm primitives. Every function in
// toolboxes/builtin/src/math/geom/geom.cpp now has an explicit typed C++ entry
// point declared here; the script-callable `*_reg` adapters delegate to
// these public functions (lifted from adapter-only over 2026-06).

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

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

/// @brief Area of the polygon with vertices `(x, y)` — `a = polyarea(x, y)`.
///
/// Shoelace formula over the closed polygon (the last vertex wraps to the
/// first; the polygon need not be explicitly closed). Fewer than 3 vertices
/// give 0.
///
/// @param x   Polygon vertex x-coordinates.
/// @param y   Polygon vertex y-coordinates (same numel as `x`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar polygon area.
/// @throws Error if `x` and `y` differ in numel.
Value polyarea(const Value &x, const Value &y,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Point-in-polygon test — `in = inpolygon(xq, yq, xv, yv)`.
///
/// For each query point `(xq[i], yq[i])` returns logical `true` if it lies
/// inside the closed polygon `(xv, yv)` (ray-casting / crossing-number).
/// The polygon need not be explicitly closed. Fewer than 3 vertices → all
/// false. The result has the shape of `xq`. (MATLAB's 2nd `on` output —
/// points exactly on the boundary — is a v1 gap; only `in` is returned.)
///
/// @param xq,yq  Query point coordinates (same numel; result takes xq's shape).
/// @param xv,yv  Polygon vertex coordinates (same numel).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Logical mask, shape of `xq`.
/// @throws Error on xq/yq or xv/yv numel mismatch.
Value inpolygon(const Value &xq, const Value &yq, const Value &xv,
                const Value &yv, std::pmr::memory_resource *mr = nullptr);

/// @brief Convex hull of a 2-D point cloud — `K = convhull(x, y)`.
///
/// Andrew's monotone-chain algorithm (O(N log N)). Returns the 1-based
/// indices of the hull vertices in CCW order, with the first vertex
/// repeated at the end (MATLAB convention). Fewer than 3 points →
/// `[1; 2; …; n; 1]`. (MATLAB's 2nd `v` output — the hull area — is a v1
/// gap; only the index vector is returned. The script form auto-plots the
/// hull when called with no output.)
///
/// @param x,y  Point coordinates (same numel).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Column of 1-based hull-vertex indices (first repeated last).
/// @throws Error on x/y numel mismatch.
Value convhull(const Value &x, const Value &y,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Boundary of a 2-D point set — `k = boundary(x, y, shrink)`.
///
/// Alpha-shape-style boundary: brute-force Delaunay, drop triangles whose
/// longest edge exceeds the `(1 - shrink)` quantile, then trace the
/// remaining boundary edges into a closed polygon (1-based indices, first
/// repeated last). `shrink == 0` (or fewer than 4 points, or a degenerate
/// triangulation) falls back to the convex hull.
///
/// @param x,y     Point coordinates (same numel).
/// @param shrink  Tightness in `[0, 1]` (clamped); `0` = convex hull, `1` =
///                tightest. Default `0.5` (MATLAB). NOTE: the script form
///                `boundary(x, y)` currently defaults to `0` (convex hull) —
///                a pre-existing engine-default gap, separate from this API.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Column of 1-based boundary-vertex indices (first repeated).
/// @throws Error on x/y numel mismatch.
Value boundary(const Value &x, const Value &y, double shrink = 0.5,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Delaunay triangulation of a 2-D point set — `T = delaunay(x, y)`.
///
/// Brute-force O(N⁴) empty-circumcircle test (fine for the small point
/// counts typical of script use). Returns an `M × 3` matrix of 1-based,
/// CCW-oriented vertex-index triples. Fewer than 3 points → `0 × 3`.
/// NOTE: the row order and per-row vertex rotation are algorithm-specific
/// and need not match MATLAB's (the triangulation is unique only for
/// points in general position).
///
/// @param x,y  Point coordinates (same numel).
/// @param mr   Memory resource (nullptr → process default).
/// @return     `M × 3` matrix of 1-based triangle vertex indices.
/// @throws Error on x/y numel mismatch.
Value delaunay(const Value &x, const Value &y,
               std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D scattered-data interpolation — `vq = griddata(x, y, v, xq, yq)`.
///
/// Interpolates the sample values `v` defined at the scattered points
/// `(x, y)` onto the query points `(xq, yq)`. Builds a brute-force Delaunay
/// triangulation of `(x, y)`, locates the triangle containing each query
/// point, and returns the barycentric-weighted value. Query points outside
/// the convex hull (and every query when fewer than 3 samples) → NaN. The
/// result takes the shape of `xq`.
///
/// Only MATLAB's default `'linear'` method is implemented; `'nearest'`,
/// `'natural'`, `'cubic'`, and `'v4'` are a v1 gap (the script form silently
/// ignores a trailing method argument — same as the original adapter).
///
/// @param x,y    Sample point coordinates (same numel as `v`).
/// @param v      Sample values at `(x, y)`.
/// @param xq,yq  Query point coordinates (same numel; result takes xq's shape).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Interpolated values, shape of `xq` (NaN outside the hull).
/// @throws Error on x/y/v numel mismatch or xq/yq numel mismatch.
Value griddata(const Value &x, const Value &y, const Value &v,
               const Value &xq, const Value &yq,
               std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D histogram bin counts over explicit edges
/// (`N = histcounts2(x, y, xedges, yedges)`).
///
/// Counts the `(x, y)` point pairs (paired by index over `min(numel)`) into
/// the rectangular bins defined by the monotonically increasing `xedges`
/// (length `nx+1`) and `yedges` (length `ny+1`), returning the `nx × ny`
/// count matrix. The last bin is right-edge-inclusive (MATLAB); points
/// outside the edge range or with non-finite coordinates are dropped. The
/// nbins / `[nx ny]` / auto-edge convenience forms are resolved by the
/// script adapter — this typed entry takes the resolved edge vectors.
///
/// @param x,y          Point coordinates (paired by index).
/// @param xedges,yedges Monotone bin edge vectors.
/// @param mr           Memory resource (nullptr → process default).
/// @return             `nx × ny` count matrix (`0 × 0` if fewer than 1 bin).
Value histcounts2(const Value &x, const Value &y, const Value &xedges,
                  const Value &yedges, std::pmr::memory_resource *mr = nullptr);

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
