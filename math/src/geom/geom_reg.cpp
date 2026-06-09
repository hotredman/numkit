// math/src/geom/geom_reg.cpp
//
// CallContext register half (Phase 2b multi-block split).
#include <numkit/core/engine.hpp>
#include <numkit/core/figure_manager.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/math/geom/geom.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
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
                     0, 0, "inpolygon", "", "numkit:inpolygon:nargin");
    outs[0] = inpolygon(args[0], args[1], args[2], args[3], ctx.engine->resource());
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
                     0, 0, "boundary", "", "numkit:boundary:nargin");
    const auto &xv = args[0];
    const auto &yv = args[1];
    const size_t n = xv.numel();
    if (yv.numel() != n)
        throw Error("boundary: x and y must have the same numel",
                     0, 0, "boundary", "", "numkit:boundary:shape");
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
    // Main alpha-shape path lives in the public C++ API.
    outs[0] = boundary(args[0], args[1], shrink, ctx.engine->resource());
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
                     0, 0, "polyarea", "", "numkit:polyarea:nargin");
    outs[0] = polyarea(args[0], args[1], ctx.engine->resource());
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
                     0, 0, "convhull", "", "numkit:convhull:nargin");
    Value K = convhull(args[0], args[1], ctx.engine->resource());
    if (nargout != 0) {
        outs[0] = std::move(K);
        return;
    }
    // Auto-plot the hull polygon when called with no LHS (MATLAB
    // convention). Reconstruct hull coordinates from the index vector K.
    auto &fm = ctx.engine->figureManager();
    fm.prepareForPlot();
    const double *k = K.doubleData();
    const std::size_t m = K.numel();
    std::ostringstream xs, ys;
    xs << '['; ys << '[';
    for (std::size_t i = 0; i < m; ++i) {
        if (i) { xs << ','; ys << ','; }
        const std::size_t kk = static_cast<std::size_t>(k[i]) - 1;
        xs << args[0].elemAsDouble(kk);
        ys << args[1].elemAsDouble(kk);
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
                     0, 0, "histcounts2", "", "numkit:histcounts2:nargin");
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

    // Wrap the resolved edges as Values and delegate to the public core.
    Value xeV = Value::matrix(1, xedges.size(), ValueType::DOUBLE, mr);
    Value yeV = Value::matrix(1, yedges.size(), ValueType::DOUBLE, mr);
    std::copy(xedges.begin(), xedges.end(), xeV.doubleDataMut());
    std::copy(yedges.begin(), yedges.end(), yeV.doubleDataMut());
    outs[0] = histcounts2(xv, yv, xeV, yeV, mr);
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
                     0, 0, "delaunay", "", "numkit:delaunay:nargin");
    outs[0] = delaunay(args[0], args[1], ctx.engine->resource());
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
                     0, 0, "griddata", "", "numkit:griddata:nargin");
    // NOTE: a trailing method arg (args[5], e.g. 'nearest') is currently
    // ignored — only 'linear' is implemented (v1 gap, see header).
    outs[0] = griddata(args[0], args[1], args[2], args[3], args[4],
                       ctx.engine->resource());
}

} // namespace detail

namespace detail {

void griddatan_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("griddatan: requires (X, v, xi [, method])",
                     0, 0, "griddatan", "", "numkit:griddatan:nargin");
    std::string method = "linear";
    if (args.size() >= 4 && args[3].isChar())
        method = args[3].toString();
    outs[0] = griddatan(args[0], args[1], args[2], method,
                        ctx.engine->resource());
}

} // namespace detail

namespace detail {

void matchpairs_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("matchpairs: requires (Cost, costUnmatched [, 'min'|'max'])",
                     0, 0, "matchpairs", "", "numkit:matchpairs:nargin");
    const double cU = args[1].toScalar();
    std::string mode = "min";
    if (args.size() >= 3 && args[2].isChar())
        mode = args[2].toString();
    auto r = matchpairs(args[0], cU, mode, ctx.engine->resource());
    outs[0] = std::move(r.M);
    if (nargout > 1) outs[1] = std::move(r.uR);
    if (nargout > 2) outs[2] = std::move(r.uC);
}

} // namespace detail

} // namespace numkit::builtin
