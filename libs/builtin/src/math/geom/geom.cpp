// libs/builtin/src/math/geom/geom.cpp
//
// Computational-geometry primitives:
//   inpolygon — point-in-polygon test (ray-casting)
//   convhull  — convex hull of a 2-D point cloud (Andrew's monotone chain)

#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/figure_manager.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
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
// 1 = tightest concave) drives an alpha-shape contraction.
//
// Algorithm:
//   1. Brute-force Delaunay triangulation.
//   2. Compute the longest-edge length of each triangle.
//   3. Threshold = quantile of edge lengths at (1 - shrink) — small
//      shrink keeps almost everything; large shrink keeps only the
//      densely-packed triangles.
//   4. Boundary edges = edges appearing in EXACTLY one kept triangle.
//   5. Chain edges into a closed polygon by walking shared vertices.
//
// shrink == 0 → all triangles kept → convex hull (same as convhull).
// shrink == 1 → tightest concave (degenerate for sparse clouds).
//
// Returns indices of boundary vertices, with first repeated at end.
void boundary_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("boundary: requires (x, y)",
                     0, 0, "boundary", "", "m:boundary:nargin");
    const auto &xv = args[0];
    const auto &yv = args[1];
    const size_t n = xv.numel();
    if (yv.numel() != n)
        throw Error("boundary: x and y must have the same numel",
                     0, 0, "boundary", "", "m:boundary:shape");
    double shrink = 0.0;
    if (args.size() >= 3) shrink = args[2].toScalar();
    if (!std::isfinite(shrink)) shrink = 0.0;
    if (shrink < 0) shrink = 0;
    if (shrink > 1) shrink = 1;
    // shrink == 0 → convex hull (use existing impl).
    if (shrink == 0.0 || n < 4) {
        std::array<Value, 2> proxied{ args[0], args[1] };
        convhull_reg(Span<const Value>(proxied.data(), 2), nargout, outs, ctx);
        return;
    }
    auto *mr = ctx.engine->resource();
    std::vector<double> X(n), Y(n);
    for (size_t i = 0; i < n; ++i) {
        X[i] = xv.elemAsDouble(i);
        Y[i] = yv.elemAsDouble(i);
    }
    // Brute-force Delaunay (same as delaunay_reg).
    auto sa2 = [&](size_t a, size_t b, size_t c) {
        return (X[b]-X[a]) * (Y[c]-Y[a]) - (Y[b]-Y[a]) * (X[c]-X[a]);
    };
    auto inC = [&](size_t a, size_t b, size_t c, size_t p) {
        const double ax=X[a]-X[p], ay=Y[a]-Y[p];
        const double bx=X[b]-X[p], by=Y[b]-Y[p];
        const double cx=X[c]-X[p], cy=Y[c]-Y[p];
        const double a2=ax*ax+ay*ay;
        const double b2=bx*bx+by*by;
        const double c2=cx*cx+cy*cy;
        return ax*(by*c2-cy*b2) - ay*(bx*c2-cx*b2) + a2*(bx*cy-cx*by);
    };
    std::vector<std::array<size_t, 3>> tris;
    for (size_t a = 0; a < n; ++a)
        for (size_t b = a + 1; b < n; ++b)
            for (size_t c = b + 1; c < n; ++c) {
                const double s = sa2(a, b, c);
                if (std::abs(s) < 1e-15) continue;
                size_t va=a, vb=b, vc=c;
                if (s < 0) std::swap(vb, vc);
                bool ok = true;
                for (size_t p = 0; p < n; ++p) {
                    if (p == va || p == vb || p == vc) continue;
                    if (inC(va, vb, vc, p) > 1e-12) { ok = false; break; }
                }
                if (ok) tris.push_back({va, vb, vc});
            }
    if (tris.empty()) {
        std::array<Value, 2> proxied{ args[0], args[1] };
        convhull_reg(Span<const Value>(proxied.data(), 2), nargout, outs, ctx);
        return;
    }
    // Longest edge per triangle.
    auto edgeLen = [&](size_t a, size_t b) {
        const double dx = X[a] - X[b], dy = Y[a] - Y[b];
        return std::sqrt(dx * dx + dy * dy);
    };
    std::vector<double> longestEdge(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) {
        const auto &T = tris[i];
        longestEdge[i] = std::max({edgeLen(T[0], T[1]),
                                   edgeLen(T[1], T[2]),
                                   edgeLen(T[2], T[0])});
    }
    // Threshold = quantile at (1 - shrink).
    std::vector<double> sortedLen = longestEdge;
    std::sort(sortedLen.begin(), sortedLen.end());
    const double q = 1.0 - shrink;
    const size_t qi = std::min(sortedLen.size() - 1,
                                (size_t)std::floor(q * sortedLen.size()));
    const double threshold = sortedLen[qi];
    // Keep triangles whose longest edge ≤ threshold.
    std::vector<std::array<size_t, 3>> kept;
    kept.reserve(tris.size());
    for (size_t i = 0; i < tris.size(); ++i)
        if (longestEdge[i] <= threshold) kept.push_back(tris[i]);
    if (kept.empty()) {
        std::array<Value, 2> proxied{ args[0], args[1] };
        convhull_reg(Span<const Value>(proxied.data(), 2), nargout, outs, ctx);
        return;
    }
    // Boundary edges = edges appearing in exactly one kept triangle.
    // Edge key: ordered (min, max) so we don't double-count direction.
    std::map<std::pair<size_t, size_t>, int> edgeCount;
    auto bumpEdge = [&](size_t a, size_t b) {
        if (a > b) std::swap(a, b);
        edgeCount[{a, b}] += 1;
    };
    for (const auto &T : kept) {
        bumpEdge(T[0], T[1]);
        bumpEdge(T[1], T[2]);
        bumpEdge(T[2], T[0]);
    }
    // Collect boundary edges into adjacency.
    std::map<size_t, std::vector<size_t>> adj;
    for (const auto &[edge, count] : edgeCount) {
        if (count == 1) {
            adj[edge.first].push_back(edge.second);
            adj[edge.second].push_back(edge.first);
        }
    }
    if (adj.empty()) {
        std::array<Value, 2> proxied{ args[0], args[1] };
        convhull_reg(Span<const Value>(proxied.data(), 2), nargout, outs, ctx);
        return;
    }
    // Walk from arbitrary boundary vertex following adj edges,
    // marking visited until we return to start.
    std::vector<size_t> poly;
    std::set<std::pair<size_t, size_t>> visited;
    const size_t start = adj.begin()->first;
    size_t cur = start;
    poly.push_back(cur);
    for (size_t step = 0; step < adj.size() + 2; ++step) {
        bool moved = false;
        for (size_t nxt : adj[cur]) {
            std::pair<size_t, size_t> e{std::min(cur, nxt), std::max(cur, nxt)};
            if (visited.count(e)) continue;
            visited.insert(e);
            poly.push_back(nxt);
            cur = nxt;
            moved = true;
            if (cur == start) { moved = false; break; }
            break;
        }
        if (!moved) break;
    }
    // Output as a column vector of 1-based indices, first repeated
    // at end (MATLAB convention).
    auto out = Value::matrix(poly.size(), 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < poly.size(); ++i)
        dst[i] = (double)(poly[i] + 1);
    outs[0] = std::move(out);
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
//
// Auto-plot: when called with nargout == 0 (no LHS), MATLAB plots the
// hull polygon — `plot(x(k), y(k))`. We mirror that by pushing a line
// dataset directly through the figure manager so users get the visual
// without needing to wire up a plot() call.
void convhull_reg(Span<const Value> args, size_t nargout,
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
    if (nargout > 0) outs[0] = std::move(out);

    // Auto-plot when no LHS — MATLAB convention.
    if (nargout == 0) {
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        for (std::size_t i = 0; i < hull.size(); ++i) {
            if (i) { xs << ','; ys << ','; }
            xs << X[hull[i]];
            ys << Y[hull[i]];
        }
        xs << ']'; ys << ']';
        DatasetInfo ds;
        ds.type  = "line";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.style = "color=#1f77b4";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
    }
}

// ── histcounts2 ──────────────────────────────────────────────────────
//
// histcounts2(x, y[, nbins | xedges, yedges]) — 2-D histogram count
// matrix. Returns a counts matrix N(nx × ny) plus optional edges.
//
// Forms supported (subset):
//   N = histcounts2(x, y)            — auto 10×10 bins over data extent
//   N = histcounts2(x, y, n)         — n×n
//   N = histcounts2(x, y, [nx ny])   — explicit grid
//   N = histcounts2(x, y, xedges, yedges)  — explicit edges
void histcounts2_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("histcounts2: requires (x, y)",
                     0, 0, "histcounts2", "", "m:histcounts2:nargin");
    const auto &xv = args[0];
    const auto &yv = args[1];
    const std::size_t n = std::min(xv.numel(), yv.numel());
    auto *mr = ctx.engine->resource();
    if (n == 0) {
        outs[0] = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return;
    }

    // Determine bin edges.
    int nx = 10, ny = 10;
    std::vector<double> xedges, yedges;
    if (args.size() == 3) {
        if (args[2].numel() == 1) {
            nx = ny = (int)args[2].toScalar();
        } else if (args[2].numel() >= 2) {
            // Could be [nx ny] (length 2) or explicit edges
            // (length > 2). MATLAB disambiguates by exactness; we
            // follow the same heuristic.
            if (args[2].numel() == 2) {
                nx = (int)args[2].elemAsDouble(0);
                ny = (int)args[2].elemAsDouble(1);
            } else {
                xedges.resize(args[2].numel());
                for (std::size_t i = 0; i < xedges.size(); ++i)
                    xedges[i] = args[2].elemAsDouble(i);
                yedges = xedges;
            }
        }
    } else if (args.size() >= 4) {
        xedges.resize(args[2].numel());
        yedges.resize(args[3].numel());
        for (std::size_t i = 0; i < xedges.size(); ++i)
            xedges[i] = args[2].elemAsDouble(i);
        for (std::size_t i = 0; i < yedges.size(); ++i)
            yedges[i] = args[3].elemAsDouble(i);
    }
    if (nx < 1) nx = 1;
    if (ny < 1) ny = 1;

    // Compute auto-edges if explicit ones not given.
    if (xedges.empty() || yedges.empty()) {
        double xmn = xv.elemAsDouble(0), xmx = xmn;
        double ymn = yv.elemAsDouble(0), ymx = ymn;
        for (std::size_t i = 1; i < n; ++i) {
            const double X = xv.elemAsDouble(i);
            const double Y = yv.elemAsDouble(i);
            if (std::isfinite(X)) {
                if (X < xmn) xmn = X;
                if (X > xmx) xmx = X;
            }
            if (std::isfinite(Y)) {
                if (Y < ymn) ymn = Y;
                if (Y > ymx) ymx = Y;
            }
        }
        if (xmx == xmn) xmx = xmn + 1.0;
        if (ymx == ymn) ymx = ymn + 1.0;
        if (xedges.empty()) {
            xedges.resize(nx + 1);
            for (int i = 0; i <= nx; ++i)
                xedges[i] = xmn + (xmx - xmn) * i / nx;
        }
        if (yedges.empty()) {
            yedges.resize(ny + 1);
            for (int i = 0; i <= ny; ++i)
                yedges[i] = ymn + (ymx - ymn) * i / ny;
        }
    }
    nx = (int)xedges.size() - 1;
    ny = (int)yedges.size() - 1;
    if (nx < 1 || ny < 1) {
        outs[0] = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return;
    }

    auto out = Value::matrix((std::size_t)nx, (std::size_t)ny,
                             ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    std::memset(dst, 0, sizeof(double) * nx * ny);

    auto findBin = [](const std::vector<double> &edges, double v) -> int {
        const int e = (int)edges.size();
        if (v < edges[0] || v > edges[e - 1]) return -1;
        // Inclusive on the right edge for the last bin (MATLAB).
        for (int i = 0; i < e - 1; ++i) {
            if (v >= edges[i] && (v < edges[i + 1]
                                  || (i == e - 2 && v == edges[i + 1])))
                return i;
        }
        return -1;
    };
    for (std::size_t i = 0; i < n; ++i) {
        const double X = xv.elemAsDouble(i);
        const double Y = yv.elemAsDouble(i);
        if (!std::isfinite(X) || !std::isfinite(Y)) continue;
        const int bx = findBin(xedges, X);
        const int by = findBin(yedges, Y);
        if (bx < 0 || by < 0) continue;
        // Column-major: dst[col * nx + row], with row=bx, col=by.
        dst[(std::size_t)by * (std::size_t)nx + (std::size_t)bx] += 1.0;
    }
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

// ── griddata ─────────────────────────────────────────────────────────
//
// griddata(x, y, v, xq, yq) — interpolate the scattered samples
// (x[i], y[i], v[i]) at the query points (xq[j], yq[j]). v1 uses
// linear barycentric interpolation over the Delaunay triangulation.
// Query points outside the convex hull get NaN.
//
// Forms supported:
//   vq = griddata(x, y, v, xq, yq)
//   vq = griddata(x, y, v, xq, yq, 'linear')   — only mode for v1
//
// 'nearest', 'natural', 'cubic', 'v4' modes are BACKLOG.
void griddata_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("griddata: requires (x, y, v, xq, yq)",
                     0, 0, "griddata", "", "m:griddata:nargin");
    const auto &xv = args[0];
    const auto &yv = args[1];
    const auto &vv = args[2];
    const auto &xq = args[3];
    const auto &yq = args[4];
    const std::size_t n = xv.numel();
    if (yv.numel() != n || vv.numel() != n)
        throw Error("griddata: x, y, v must have the same numel",
                     0, 0, "griddata", "", "m:griddata:shape");
    if (xq.numel() != yq.numel())
        throw Error("griddata: xq and yq must have the same numel",
                     0, 0, "griddata", "", "m:griddata:queryShape");
    auto *mr = ctx.engine->resource();
    auto out = Value::matrix(xq.dims().rows(), xq.dims().cols(),
                             ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const std::size_t nq = xq.numel();
    if (n < 3) {
        for (std::size_t i = 0; i < nq; ++i) dst[i] = std::nan("");
        outs[0] = std::move(out);
        return;
    }

    ScratchArena scratch(mr);
    ScratchVec<double> X(n, &scratch), Y(n, &scratch), V(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        X[i] = xv.elemAsDouble(i);
        Y[i] = yv.elemAsDouble(i);
        V[i] = vv.elemAsDouble(i);
    }
    // Reuse the brute-force Delaunay logic — emit triangle list as
    // index triples.
    auto signedArea2 = [&](std::size_t a, std::size_t b, std::size_t c) {
        return (X[b] - X[a]) * (Y[c] - Y[a]) - (Y[b] - Y[a]) * (X[c] - X[a]);
    };
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
    std::vector<std::array<std::size_t, 3>> tris;
    tris.reserve(2 * n);
    for (std::size_t a = 0; a < n; ++a) {
        for (std::size_t b = a + 1; b < n; ++b) {
            for (std::size_t c = b + 1; c < n; ++c) {
                const double sa2 = signedArea2(a, b, c);
                if (std::abs(sa2) < 1e-15) continue;
                std::size_t va = a, vb = b, vc = c;
                if (sa2 < 0) std::swap(vb, vc);
                bool ok = true;
                for (std::size_t p = 0; p < n; ++p) {
                    if (p == va || p == vb || p == vc) continue;
                    if (inCircle(va, vb, vc, p) > 1e-12) { ok = false; break; }
                }
                if (ok) tris.push_back({ va, vb, vc });
            }
        }
    }
    // Per query point: walk triangles, find one that contains the
    // query, compute barycentric coords, interpolate v.
    for (std::size_t q = 0; q < nq; ++q) {
        const double Xq = xq.elemAsDouble(q);
        const double Yq = yq.elemAsDouble(q);
        bool found = false;
        for (const auto &t : tris) {
            const double xa = X[t[0]], ya = Y[t[0]];
            const double xb = X[t[1]], yb = Y[t[1]];
            const double xc = X[t[2]], yc = Y[t[2]];
            const double denom = (yb - yc) * (xa - xc) + (xc - xb) * (ya - yc);
            if (std::abs(denom) < 1e-15) continue;
            const double l1 = ((yb - yc) * (Xq - xc) + (xc - xb) * (Yq - yc)) / denom;
            const double l2 = ((yc - ya) * (Xq - xc) + (xa - xc) * (Yq - yc)) / denom;
            const double l3 = 1.0 - l1 - l2;
            // Inside or on boundary (allow a tiny epsilon for FP noise).
            if (l1 >= -1e-9 && l2 >= -1e-9 && l3 >= -1e-9) {
                dst[q] = l1 * V[t[0]] + l2 * V[t[1]] + l3 * V[t[2]];
                found = true;
                break;
            }
        }
        if (!found) dst[q] = std::nan("");
    }
    outs[0] = std::move(out);
}

// ── griddatan ────────────────────────────────────────────────────────
//
// griddatan(X, v, xi [, method]) — N-D scattered-data interpolation.
//   X  is m×n  (m data points in n-dim space)
//   v  is m×1  (values at those points)
//   xi is k×n  (k query points)
// Returns vi (k×1).
//
// v1 method support:
//   'nearest' — nearest-neighbour by Euclidean distance. Works for
//               any n. O(m·k·n) brute-force search; fine for typical
//               sizes (full kd-tree is a future-work backlog item).
//   'linear'  — only n == 2 is supported (delegates to the same
//               brute-force Delaunay code as griddata). KNOWN GAP:
//               n ≥ 3 linear needs a real N-D Delaunay (Qhull-style),
//               which is not in v1.
//
// Default method is 'linear' (MATLAB-compatible) — so a call without
// the method arg on n ≥ 3 errors out with a clear pointer to the gap.
void griddatan_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("griddatan: requires (X, v, xi [, method])",
                     0, 0, "griddatan", "", "m:griddatan:nargin");
    const auto &Xv = args[0];
    const auto &vv = args[1];
    const auto &xi = args[2];
    std::string method = "linear";
    if (args.size() >= 4 && args[3].isChar())
        method = args[3].toString();

    // Shape: X is m×n, xi is k×n. Use rows/cols directly so a row
    // vector `xi = [a b]` reads as 1×n (one query point in n-D), and
    // a column vector `xi = [a; b]` reads as 2×1 (two queries in 1-D).
    const std::size_t m = Xv.dims().rows();
    const std::size_t n = Xv.dims().cols();
    if (vv.numel() != m)
        throw Error("griddatan: length(v) must equal rows(X)",
                     0, 0, "griddatan", "", "m:griddatan:shape");
    const std::size_t k    = xi.dims().rows();
    const std::size_t nQry = xi.dims().cols();
    if (nQry != n)
        throw Error("griddatan: cols(xi) must equal cols(X)",
                     0, 0, "griddatan", "", "m:griddatan:queryDim");

    auto *mr = ctx.engine->resource();
    auto out = Value::matrix(k, 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();

    if (method == "nearest") {
        // Brute-force NN. Read X column-major as m × n.
        ScratchArena scratch(mr);
        ScratchVec<double> Xd(m * n, &scratch);
        for (std::size_t r = 0; r < m; ++r)
            for (std::size_t c = 0; c < n; ++c)
                Xd[c * m + r] = Xv.elemAsDouble(c * m + r);
        ScratchVec<double> Vd(m, &scratch);
        for (std::size_t i = 0; i < m; ++i) Vd[i] = vv.elemAsDouble(i);
        for (std::size_t q = 0; q < k; ++q) {
            double best = std::numeric_limits<double>::infinity();
            std::size_t bestIdx = 0;
            for (std::size_t i = 0; i < m; ++i) {
                double d2 = 0.0;
                for (std::size_t c = 0; c < n; ++c) {
                    const double qc = xi.elemAsDouble(c * k + q);
                    const double dc = qc - Xd[c * m + i];
                    d2 += dc * dc;
                }
                if (d2 < best) { best = d2; bestIdx = i; }
            }
            dst[q] = Vd[bestIdx];
        }
        outs[0] = std::move(out);
        return;
    }

    if (method == "linear") {
        if (n != 2)
            throw Error("griddatan: 'linear' method requires n == 2 (use "
                        "'nearest' for higher dimensions; N-D Delaunay "
                        "is a v1 KNOWN GAP)",
                         0, 0, "griddatan", "",
                         "m:griddatan:linearNDUnsupported");
        // Delegate to griddata-style barycentric. Repack: X(:,1) = x,
        // X(:,2) = y, then reuse the brute-force logic by constructing
        // a temporary call.
        // Quick inline implementation since the input shape differs.
        ScratchArena scratch(mr);
        ScratchVec<double> X(m, &scratch), Y(m, &scratch), V(m, &scratch);
        for (std::size_t i = 0; i < m; ++i) {
            X[i] = Xv.elemAsDouble(0 * m + i);
            Y[i] = Xv.elemAsDouble(1 * m + i);
            V[i] = vv.elemAsDouble(i);
        }
        if (m < 3) {
            for (std::size_t q = 0; q < k; ++q) dst[q] = std::nan("");
            outs[0] = std::move(out);
            return;
        }
        auto signedArea2 = [&](std::size_t a, std::size_t b, std::size_t c) {
            return (X[b] - X[a]) * (Y[c] - Y[a]) - (Y[b] - Y[a]) * (X[c] - X[a]);
        };
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
        std::vector<std::array<std::size_t, 3>> tris;
        tris.reserve(2 * m);
        for (std::size_t a = 0; a < m; ++a)
            for (std::size_t b = a + 1; b < m; ++b)
                for (std::size_t c = b + 1; c < m; ++c) {
                    const double sa2 = signedArea2(a, b, c);
                    if (std::abs(sa2) < 1e-15) continue;
                    std::size_t va = a, vb = b, vc = c;
                    if (sa2 < 0) std::swap(vb, vc);
                    bool ok = true;
                    for (std::size_t p = 0; p < m; ++p) {
                        if (p == va || p == vb || p == vc) continue;
                        if (inCircle(va, vb, vc, p) > 1e-12) { ok = false; break; }
                    }
                    if (ok) tris.push_back({ va, vb, vc });
                }
        for (std::size_t q = 0; q < k; ++q) {
            const double Xq = xi.elemAsDouble(0 * k + q);
            const double Yq = xi.elemAsDouble(1 * k + q);
            bool found = false;
            for (const auto &t : tris) {
                const double xa = X[t[0]], ya = Y[t[0]];
                const double xb = X[t[1]], yb = Y[t[1]];
                const double xc = X[t[2]], yc = Y[t[2]];
                const double denom = (yb - yc) * (xa - xc) + (xc - xb) * (ya - yc);
                if (std::abs(denom) < 1e-15) continue;
                const double l1 = ((yb - yc) * (Xq - xc) + (xc - xb) * (Yq - yc)) / denom;
                const double l2 = ((yc - ya) * (Xq - xc) + (xa - xc) * (Yq - yc)) / denom;
                const double l3 = 1.0 - l1 - l2;
                if (l1 >= -1e-9 && l2 >= -1e-9 && l3 >= -1e-9) {
                    dst[q] = l1 * V[t[0]] + l2 * V[t[1]] + l3 * V[t[2]];
                    found = true;
                    break;
                }
            }
            if (!found) dst[q] = std::nan("");
        }
        outs[0] = std::move(out);
        return;
    }

    throw Error("griddatan: unknown method '" + method
                + "' (supported: 'linear', 'nearest')",
                 0, 0, "griddatan", "", "m:griddatan:badMethod");
}

} // namespace detail

} // namespace numkit::builtin
