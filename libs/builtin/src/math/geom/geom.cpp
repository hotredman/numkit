// libs/builtin/src/math/geom/geom.cpp
//
// Computational-geometry primitives:
//   inpolygon — point-in-polygon test (ray-casting)
//   convhull  — convex hull of a 2-D point cloud (Andrew's monotone chain)

#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace numkit::builtin {

namespace detail {

// ── inpolygon ────────────────────────────────────────────────────────
//
// inpolygon(xq, yq, xv, yv) — for each query point (xq[i], yq[i]),
// returns logical true if it is INSIDE the closed polygon defined by
// (xv, yv). Uses the standard ray-casting (crossing-number) algorithm:
// fire a horizontal ray to +∞ from the query point and count edge
// crossings — odd = inside.
//
// MATLAB also returns a second `on` output for points exactly on the
// boundary; v1 returns just the `in` mask (boundary points get
// classified by the strict-inequality version, treated as inside).
//
// Polygon need not be explicitly closed (xv(end) == xv(1)); the
// algorithm wraps from the last vertex back to the first.
void inpolygon_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("inpolygon: requires (xq, yq, xv, yv)",
                     0, 0, "inpolygon", "", "m:inpolygon:nargin");
    const auto &xq = args[0];
    const auto &yq = args[1];
    const auto &xv = args[2];
    const auto &yv = args[3];
    if (xq.numel() != yq.numel())
        throw Error("inpolygon: xq and yq must have the same numel",
                     0, 0, "inpolygon", "", "m:inpolygon:queryShape");
    if (xv.numel() != yv.numel())
        throw Error("inpolygon: xv and yv must have the same numel",
                     0, 0, "inpolygon", "", "m:inpolygon:polyShape");

    auto *mr = ctx.engine->resource();
    const std::size_t nQ = xq.numel();
    const std::size_t nV = xv.numel();
    auto out = Value::matrix(xq.dims().rows(), xq.dims().cols(),
                             ValueType::LOGICAL, mr);
    uint8_t *dst = out.logicalDataMut();
    if (nV < 3) {
        std::memset(dst, 0, nQ);
        outs[0] = std::move(out);
        return;
    }

    ScratchArena scratch(mr);
    ScratchVec<double> px(nV, &scratch);
    ScratchVec<double> py(nV, &scratch);
    for (std::size_t i = 0; i < nV; ++i) {
        px[i] = xv.elemAsDouble(i);
        py[i] = yv.elemAsDouble(i);
    }

    for (std::size_t q = 0; q < nQ; ++q) {
        const double X = xq.elemAsDouble(q);
        const double Y = yq.elemAsDouble(q);
        bool inside = false;
        std::size_t j = nV - 1;
        for (std::size_t i = 0; i < nV; ++i) {
            const double xi = px[i], yi = py[i];
            const double xj = px[j], yj = py[j];
            // Edge straddles the horizontal line at y=Y?
            const bool straddles = (yi > Y) != (yj > Y);
            if (straddles) {
                // X-coordinate of the edge's intersection with that line
                const double xCross = xi + (Y - yi) * (xj - xi) / (yj - yi);
                if (X < xCross) inside = !inside;
            }
            j = i;
        }
        dst[q] = inside ? 1 : 0;
    }
    outs[0] = std::move(out);
}

// Forward decl — convhull_reg defined later in this file, used by
// boundary_reg below.
void convhull_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx);

// ── boundary ─────────────────────────────────────────────────────────
//
// boundary(x, y [, shrink]) — single boundary polygon around a 2-D
// point cloud. MATLAB's `shrink` parameter (0 = convex hull,
// 1 = tightest concave) drives an alpha-shape contraction; v1
// implements only the convex case (≡ convhull) and ignores shrink.
//
// Returns indices of boundary vertices, CCW with first repeated.
void boundary_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("boundary: requires (x, y)",
                     0, 0, "boundary", "", "m:boundary:nargin");
    std::array<Value, 2> proxied{ args[0], args[1] };
    convhull_reg(Span<const Value>(proxied.data(), 2), nargout, outs, ctx);
}

// ── polyarea ─────────────────────────────────────────────────────────
//
// polyarea(x, y) — signed-then-absolute area of the simple polygon
// with vertices (x, y), via the shoelace formula:
//
//   A = (1/2) · |Σ (x[i]·y[i+1] - x[i+1]·y[i])|
//
// Polygon may be unclosed (algorithm wraps the last vertex back to
// the first). Returns 0 if fewer than 3 vertices.
void polyarea_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyarea: requires (x, y)",
                     0, 0, "polyarea", "", "m:polyarea:nargin");
    const auto &xv = args[0];
    const auto &yv = args[1];
    const std::size_t n = xv.numel();
    if (yv.numel() != n)
        throw Error("polyarea: x and y must have the same numel",
                     0, 0, "polyarea", "", "m:polyarea:shape");
    auto *mr = ctx.engine->resource();
    if (n < 3) {
        outs[0] = Value::scalar(0.0, mr);
        return;
    }
    double s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        s += xv.elemAsDouble(i) * yv.elemAsDouble(j)
           - xv.elemAsDouble(j) * yv.elemAsDouble(i);
    }
    outs[0] = Value::scalar(0.5 * std::abs(s), mr);
}

// ── convhull ─────────────────────────────────────────────────────────
//
// convhull(x, y) — indices of the convex hull vertices of the 2-D
// point cloud, in CCW order, with the first vertex repeated at the
// end (MATLAB convention). Andrew's monotone-chain algorithm:
// O(N log N) sort + O(N) scan.
void convhull_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convhull: requires (x, y)",
                     0, 0, "convhull", "", "m:convhull:nargin");
    const auto &xv = args[0];
    const auto &yv = args[1];
    const std::size_t n = xv.numel();
    if (yv.numel() != n)
        throw Error("convhull: x and y must have the same numel",
                     0, 0, "convhull", "", "m:convhull:shape");
    auto *mr = ctx.engine->resource();
    if (n < 3) {
        // Degenerate — return [1, 2, ..., n, 1] so the polygon wraps.
        auto out = Value::matrix(n + 1, 1, ValueType::DOUBLE, mr);
        double *dst = out.doubleDataMut();
        for (std::size_t i = 0; i < n; ++i) dst[i] = static_cast<double>(i + 1);
        dst[n] = 1.0;
        outs[0] = std::move(out);
        return;
    }

    ScratchArena scratch(mr);
    ScratchVec<std::size_t> idx(n, &scratch);
    ScratchVec<double> X(n, &scratch);
    ScratchVec<double> Y(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        X[i] = xv.elemAsDouble(i);
        Y[i] = yv.elemAsDouble(i);
        idx[i] = i;
    }
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        if (X[a] != X[b]) return X[a] < X[b];
        return Y[a] < Y[b];
    });

    auto cross = [&](std::size_t o, std::size_t a, std::size_t b) {
        return (X[a] - X[o]) * (Y[b] - Y[o]) - (Y[a] - Y[o]) * (X[b] - X[o]);
    };

    // Build lower hull.
    ScratchVec<std::size_t> hull(&scratch);
    hull.reserve(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t p = idx[i];
        while (hull.size() >= 2
               && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    // Upper hull.
    const std::size_t lowerSize = hull.size() + 1;
    for (std::size_t i = n - 1; i-- > 0; ) {
        const std::size_t p = idx[i];
        while (hull.size() >= lowerSize
               && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    // hull now ends with the start point repeated; that's MATLAB's
    // convention.
    auto out = Value::matrix(hull.size(), 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < hull.size(); ++i)
        dst[i] = static_cast<double>(hull[i] + 1);   // 1-based
    outs[0] = std::move(out);
}

// ── delaunay ─────────────────────────────────────────────────────────
//
// delaunay(x, y) — Delaunay triangulation indices. Returns an M×3
// matrix where each row is a triangle's three 1-based vertex indices.
//
// v1 uses an O(N⁴) brute-force in-circle test: for every triple of
// distinct points (a, b, c), check if no fourth point lies strictly
// inside their circumcircle — if true, the triple is a Delaunay
// triangle. Cheap and clear for the small / mid-N cases typical of
// MATLAB scripts; for N > ~50 a proper Bowyer-Watson incremental
// algorithm is BACKLOG.
//
// Triangles are emitted with CCW orientation (negative signed-area
// triples are reordered).
void delaunay_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("delaunay: requires (x, y)",
                     0, 0, "delaunay", "", "m:delaunay:nargin");
    const auto &xv = args[0];
    const auto &yv = args[1];
    const std::size_t n = xv.numel();
    if (yv.numel() != n)
        throw Error("delaunay: x and y must have the same numel",
                     0, 0, "delaunay", "", "m:delaunay:shape");
    auto *mr = ctx.engine->resource();
    if (n < 3) {
        outs[0] = Value::matrix(0, 3, ValueType::DOUBLE, mr);
        return;
    }

    ScratchArena scratch(mr);
    ScratchVec<double> X(n, &scratch);
    ScratchVec<double> Y(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        X[i] = xv.elemAsDouble(i);
        Y[i] = yv.elemAsDouble(i);
    }

    // Robert Lewis-style in-circle test via 3×3 determinant. Returns
    // > 0 when P lies strictly inside the CCW-oriented circumcircle
    // of (A, B, C), < 0 if outside, 0 on the boundary.
    auto inCircle = [&](std::size_t a, std::size_t b, std::size_t c,
                        std::size_t p) {
        const double ax = X[a] - X[p], ay = Y[a] - Y[p];
        const double bx = X[b] - X[p], by = Y[b] - Y[p];
        const double cx = X[c] - X[p], cy = Y[c] - Y[p];
        const double a2 = ax * ax + ay * ay;
        const double b2 = bx * bx + by * by;
        const double c2 = cx * cx + cy * cy;
        return ax * (by * c2 - cy * b2)
             - ay * (bx * c2 - cx * b2)
             + a2 * (bx * cy - cx * by);
    };
    auto signedArea2 = [&](std::size_t a, std::size_t b, std::size_t c) {
        return (X[b] - X[a]) * (Y[c] - Y[a]) - (Y[b] - Y[a]) * (X[c] - X[a]);
    };

    std::vector<std::array<std::size_t, 3>> tris;
    tris.reserve(2 * n);
    for (std::size_t a = 0; a < n; ++a) {
        for (std::size_t b = a + 1; b < n; ++b) {
            for (std::size_t c = b + 1; c < n; ++c) {
                const double sa2 = signedArea2(a, b, c);
                if (std::abs(sa2) < 1e-15) continue;   // collinear
                std::size_t va = a, vb = b, vc = c;
                if (sa2 < 0) std::swap(vb, vc);   // make CCW
                bool ok = true;
                for (std::size_t p = 0; p < n; ++p) {
                    if (p == va || p == vb || p == vc) continue;
                    if (inCircle(va, vb, vc, p) > 1e-12) {
                        ok = false;
                        break;
                    }
                }
                if (ok) tris.push_back({ va, vb, vc });
            }
        }
    }

    auto out = Value::matrix(tris.size(), 3, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    // Column-major: dst[col * M + row].
    const std::size_t M = tris.size();
    for (std::size_t i = 0; i < M; ++i) {
        dst[0 * M + i] = static_cast<double>(tris[i][0] + 1);
        dst[1 * M + i] = static_cast<double>(tris[i][1] + 1);
        dst[2 * M + i] = static_cast<double>(tris[i][2] + 1);
    }
    outs[0] = std::move(out);
}

} // namespace detail

} // namespace numkit::builtin
