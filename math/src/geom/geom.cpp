// math/src/geom/geom.cpp
//
// Computational-geometry primitives:
//   inpolygon — point-in-polygon test (ray-casting)
//   convhull  — convex hull of a 2-D point cloud (Andrew's monotone chain)

#include <numkit/builtin/library.hpp>
#include <numkit/math/geom/geom.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace numkit::math {


// ── polyarea (public C++ API) ────────────────────────────────────────

Value polyarea(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (y.numel() != n)
        throw Error("polyarea: x and y must have the same numel",
                     0, 0, "polyarea", "", "numkit:polyarea:shape");
    if (n < 3)
        return Value::scalar(0.0, mr);
    double s = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        s += x.elemAsDouble(i) * y.elemAsDouble(j)
           - x.elemAsDouble(j) * y.elemAsDouble(i);
    }
    return Value::scalar(0.5 * std::abs(s), mr);
}

// ── inpolygon (public C++ API) ───────────────────────────────────────

Value inpolygon(const Value &xq, const Value &yq, const Value &xv,
                const Value &yv, std::pmr::memory_resource *mr)
{
    if (xq.numel() != yq.numel())
        throw Error("inpolygon: xq and yq must have the same numel",
                     0, 0, "inpolygon", "", "numkit:inpolygon:queryShape");
    if (xv.numel() != yv.numel())
        throw Error("inpolygon: xv and yv must have the same numel",
                     0, 0, "inpolygon", "", "numkit:inpolygon:polyShape");
    const std::size_t nQ = xq.numel();
    const std::size_t nV = xv.numel();
    auto out = Value::matrix(xq.dims().rows(), xq.dims().cols(),
                             ValueType::LOGICAL, mr);
    uint8_t *dst = out.logicalDataMut();
    if (nV < 3) {
        std::memset(dst, 0, nQ);
        return out;
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
            if ((yi > Y) != (yj > Y)) {
                const double xCross = xi + (Y - yi) * (xj - xi) / (yj - yi);
                if (X < xCross) inside = !inside;
            }
            j = i;
        }
        dst[q] = inside ? 1 : 0;
    }
    return out;
}

// ── convhull (public C++ API) ────────────────────────────────────────

Value convhull(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (y.numel() != n)
        throw Error("convhull: x and y must have the same numel",
                     0, 0, "convhull", "", "numkit:convhull:shape");
    if (n < 3) {
        // Degenerate — return [1, 2, ..., n, 1] so the polygon wraps.
        auto out = Value::matrix(n + 1, 1, ValueType::DOUBLE, mr);
        double *dst = out.doubleDataMut();
        for (std::size_t i = 0; i < n; ++i) dst[i] = static_cast<double>(i + 1);
        dst[n] = 1.0;
        return out;
    }
    ScratchArena scratch(mr);
    ScratchVec<std::size_t> idx(n, &scratch);
    ScratchVec<double> X(n, &scratch);
    ScratchVec<double> Y(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        X[i] = x.elemAsDouble(i);
        Y[i] = y.elemAsDouble(i);
        idx[i] = i;
    }
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        if (X[a] != X[b]) return X[a] < X[b];
        return Y[a] < Y[b];
    });
    auto cross = [&](std::size_t o, std::size_t a, std::size_t b) {
        return (X[a] - X[o]) * (Y[b] - Y[o]) - (Y[a] - Y[o]) * (X[b] - X[o]);
    };
    ScratchVec<std::size_t> hull(&scratch);
    hull.reserve(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t p = idx[i];
        while (hull.size() >= 2
               && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    const std::size_t lowerSize = hull.size() + 1;
    for (std::size_t i = n - 1; i-- > 0; ) {
        const std::size_t p = idx[i];
        while (hull.size() >= lowerSize
               && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    auto out = Value::matrix(hull.size(), 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < hull.size(); ++i)
        dst[i] = static_cast<double>(hull[i] + 1); // 1-based
    return out;
}

// ── boundary (public C++ API) ────────────────────────────────────────

Value boundary(const Value &x, const Value &y, double shrink,
               std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (y.numel() != n)
        throw Error("boundary: x and y must have the same numel",
                     0, 0, "boundary", "", "numkit:boundary:shape");
    if (!std::isfinite(shrink)) shrink = 0.0;
    if (shrink < 0) shrink = 0;
    if (shrink > 1) shrink = 1;
    // shrink == 0 (or too few points) → convex hull.
    if (shrink == 0.0 || n < 4)
        return convhull(x, y, mr);

    std::vector<double> X(n), Y(n);
    for (std::size_t i = 0; i < n; ++i) {
        X[i] = x.elemAsDouble(i);
        Y[i] = y.elemAsDouble(i);
    }
    auto sa2 = [&](std::size_t a, std::size_t b, std::size_t c) {
        return (X[b]-X[a]) * (Y[c]-Y[a]) - (Y[b]-Y[a]) * (X[c]-X[a]);
    };
    auto inC = [&](std::size_t a, std::size_t b, std::size_t c, std::size_t p) {
        const double ax=X[a]-X[p], ay=Y[a]-Y[p];
        const double bx=X[b]-X[p], by=Y[b]-Y[p];
        const double cx=X[c]-X[p], cy=Y[c]-Y[p];
        const double a2=ax*ax+ay*ay, b2=bx*bx+by*by, c2=cx*cx+cy*cy;
        return ax*(by*c2-cy*b2) - ay*(bx*c2-cx*b2) + a2*(bx*cy-cx*by);
    };
    std::vector<std::array<std::size_t, 3>> tris;
    for (std::size_t a = 0; a < n; ++a)
        for (std::size_t b = a + 1; b < n; ++b)
            for (std::size_t c = b + 1; c < n; ++c) {
                const double s = sa2(a, b, c);
                if (std::abs(s) < 1e-15) continue;
                std::size_t va=a, vb=b, vc=c;
                if (s < 0) std::swap(vb, vc);
                bool ok = true;
                for (std::size_t p = 0; p < n; ++p) {
                    if (p == va || p == vb || p == vc) continue;
                    if (inC(va, vb, vc, p) > 1e-12) { ok = false; break; }
                }
                if (ok) tris.push_back({va, vb, vc});
            }
    if (tris.empty())
        return convhull(x, y, mr);
    auto edgeLen = [&](std::size_t a, std::size_t b) {
        const double dx = X[a] - X[b], dy = Y[a] - Y[b];
        return std::sqrt(dx * dx + dy * dy);
    };
    std::vector<double> longestEdge(tris.size());
    for (std::size_t i = 0; i < tris.size(); ++i) {
        const auto &T = tris[i];
        longestEdge[i] = std::max({edgeLen(T[0], T[1]),
                                   edgeLen(T[1], T[2]),
                                   edgeLen(T[2], T[0])});
    }
    std::vector<double> sortedLen = longestEdge;
    std::sort(sortedLen.begin(), sortedLen.end());
    const double q = 1.0 - shrink;
    const std::size_t qi = std::min(sortedLen.size() - 1,
                                    (std::size_t)std::floor(q * sortedLen.size()));
    const double threshold = sortedLen[qi];
    std::vector<std::array<std::size_t, 3>> kept;
    kept.reserve(tris.size());
    for (std::size_t i = 0; i < tris.size(); ++i)
        if (longestEdge[i] <= threshold) kept.push_back(tris[i]);
    if (kept.empty())
        return convhull(x, y, mr);
    std::map<std::pair<std::size_t, std::size_t>, int> edgeCount;
    auto bumpEdge = [&](std::size_t a, std::size_t b) {
        if (a > b) std::swap(a, b);
        edgeCount[{a, b}] += 1;
    };
    for (const auto &T : kept) {
        bumpEdge(T[0], T[1]); bumpEdge(T[1], T[2]); bumpEdge(T[2], T[0]);
    }
    std::map<std::size_t, std::vector<std::size_t>> adj;
    for (const auto &[edge, count] : edgeCount)
        if (count == 1) {
            adj[edge.first].push_back(edge.second);
            adj[edge.second].push_back(edge.first);
        }
    if (adj.empty())
        return convhull(x, y, mr);
    std::vector<std::size_t> poly;
    std::set<std::pair<std::size_t, std::size_t>> visited;
    const std::size_t start = adj.begin()->first;
    std::size_t cur = start;
    poly.push_back(cur);
    for (std::size_t step = 0; step < adj.size() + 2; ++step) {
        bool moved = false;
        for (std::size_t nxt : adj[cur]) {
            std::pair<std::size_t, std::size_t> e{std::min(cur, nxt), std::max(cur, nxt)};
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
    auto out = Value::matrix(poly.size(), 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < poly.size(); ++i)
        dst[i] = static_cast<double>(poly[i] + 1);
    return out;
}

// ── delaunay ─────────────────────────────────────────────────────────
//
// See header for the public C++ API + Doxygen. This source unit hosts
// the implementation plus its adapter.

Value delaunay(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (y.numel() != n)
        throw Error("delaunay: x and y must have the same numel",
                     0, 0, "delaunay", "", "numkit:delaunay:shape");
    if (n < 3)
        return Value::matrix(0, 3, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> X(n, &scratch);
    ScratchVec<double> Y(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        X[i] = x.elemAsDouble(i);
        Y[i] = y.elemAsDouble(i);
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
    return out;
}

// ── griddata (2-D) ───────────────────────────────────────────────────
//
// See header for the public C++ API + Doxygen. Linear-only scattered
// interpolation; this source unit hosts the implementation plus its
// adapter.

Value griddata(const Value &x, const Value &y, const Value &v,
               const Value &xq, const Value &yq, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (y.numel() != n || v.numel() != n)
        throw Error("griddata: x, y, v must have the same numel",
                     0, 0, "griddata", "", "numkit:griddata:shape");
    if (xq.numel() != yq.numel())
        throw Error("griddata: xq and yq must have the same numel",
                     0, 0, "griddata", "", "numkit:griddata:queryShape");
    auto out = Value::matrix(xq.dims().rows(), xq.dims().cols(),
                             ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const std::size_t nq = xq.numel();
    if (n < 3) {
        for (std::size_t i = 0; i < nq; ++i) dst[i] = std::nan("");
        return out;
    }

    ScratchArena scratch(mr);
    ScratchVec<double> X(n, &scratch), Y(n, &scratch), V(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        X[i] = x.elemAsDouble(i);
        Y[i] = y.elemAsDouble(i);
        V[i] = v.elemAsDouble(i);
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
    return out;
}

// ── histcounts2 (over explicit edges) ─────────────────────────────────
//
// See header for the public C++ API + Doxygen. This typed core bins the
// (x, y) pairs into the bins defined by explicit edge vectors; the nbins /
// auto-edge convenience forms are resolved in the adapter below.

Value histcounts2(const Value &x, const Value &y, const Value &xedgesV,
                  const Value &yedgesV, std::pmr::memory_resource *mr)
{
    const std::size_t n = std::min(x.numel(), y.numel());
    std::vector<double> xedges(xedgesV.numel()), yedges(yedgesV.numel());
    for (std::size_t i = 0; i < xedges.size(); ++i)
        xedges[i] = xedgesV.elemAsDouble(i);
    for (std::size_t i = 0; i < yedges.size(); ++i)
        yedges[i] = yedgesV.elemAsDouble(i);

    const int nx = static_cast<int>(xedges.size()) - 1;
    const int ny = static_cast<int>(yedges.size()) - 1;
    if (nx < 1 || ny < 1 || n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    auto out = Value::matrix(static_cast<std::size_t>(nx),
                             static_cast<std::size_t>(ny), ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    std::memset(dst, 0, sizeof(double) * static_cast<std::size_t>(nx) * ny);

    auto findBin = [](const std::vector<double> &edges, double v) -> int {
        const int e = static_cast<int>(edges.size());
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
        const double X = x.elemAsDouble(i);
        const double Y = y.elemAsDouble(i);
        if (!std::isfinite(X) || !std::isfinite(Y)) continue;
        const int bx = findBin(xedges, X);
        const int by = findBin(yedges, Y);
        if (bx < 0 || by < 0) continue;
        // Column-major: dst[col * nx + row], row = bx, col = by.
        dst[static_cast<std::size_t>(by) * static_cast<std::size_t>(nx) +
            static_cast<std::size_t>(bx)] += 1.0;
    }
    return out;
}

// ── griddatan ────────────────────────────────────────────────────────
//
// See header (toolboxes/builtin/include/numkit/builtin/math/geom/geom.hpp)
// for the public C++ API + Doxygen. This source unit hosts the
// implementation plus its adapter.

Value griddatan(const Value &Xv, const Value &vv, const Value &xi,
                const std::string &method, std::pmr::memory_resource *mr)
{
    // Shape: X is m×n, xi is k×n. Use rows/cols directly so a row
    // vector `xi = [a b]` reads as 1×n (one query point in n-D), and
    // a column vector `xi = [a; b]` reads as 2×1 (two queries in 1-D).
    const std::size_t m = Xv.dims().rows();
    const std::size_t n = Xv.dims().cols();
    if (vv.numel() != m)
        throw Error("griddatan: length(v) must equal rows(X)",
                     0, 0, "griddatan", "", "numkit:griddatan:shape");
    const std::size_t k    = xi.dims().rows();
    const std::size_t nQry = xi.dims().cols();
    if (nQry != n)
        throw Error("griddatan: cols(xi) must equal cols(X)",
                     0, 0, "griddatan", "", "numkit:griddatan:queryDim");
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
        return out;
    }

    if (method == "linear") {
        if (n != 2)
            throw Error("griddatan: 'linear' method requires n == 2 (use "
                        "'nearest' for higher dimensions; N-D Delaunay "
                        "is a v1 KNOWN GAP)",
                         0, 0, "griddatan", "",
                         "numkit:griddatan:linearNDUnsupported");
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
            return out;
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
        return out;
    }

    throw Error("griddatan: unknown method '" + method
                + "' (supported: 'linear', 'nearest')",
                 0, 0, "griddatan", "", "numkit:griddatan:badMethod");
}


// ── matchpairs — linear assignment / bipartite matching ─────────────
//
// See header (toolboxes/builtin/include/numkit/builtin/math/geom/geom.hpp)
// for the public C++ API + Doxygen.
//
// Algorithm: classical O(N²·M) Jonker-Volgenant Hungarian on the
// augmented (rows + cols) × (rows + cols) cost matrix:
//
//   ┌──────────────────────┬─────────────────────┐
//   │      Cost(rows×cols) │ diag(costUnmatched) │
//   │                      │ + INF elsewhere     │   rows × cols  +  rows × rows
//   ├──────────────────────┼─────────────────────┤
//   │ diag(costUnmatched)  │   zero block        │
//   │ + INF elsewhere      │  (dummy-dummy free) │   cols × cols  +  cols × rows
//   └──────────────────────┴─────────────────────┘
//
// Each real match (i, j) costs Cost[i][j]; leaving row i unmatched
// costs `costUnmatched` (matched to its private row-dummy col); same
// for col j (matched to its private col-dummy row); free dummy-dummy
// matches absorb the leftover capacity.
//
// 'max' mode negates both Cost AND costUnmatched (MATLAB convention —
// costUnmatched becomes a REWARD for leaving unmatched).
MatchpairsResult matchpairs(const Value &C, double cU,
                            const std::string &mode,
                            std::pmr::memory_resource *mr)
{
    if (C.type() == ValueType::COMPLEX)
        throw Error("matchpairs: complex Cost not supported",
                     0, 0, "matchpairs", "", "numkit:matchpairs:complex");
    bool maximise = false;
    if (mode == "max") maximise = true;
    else if (!mode.empty() && mode != "min")
        throw Error("matchpairs: mode must be 'min' or 'max'",
                     0, 0, "matchpairs", "", "numkit:matchpairs:badMode");

    const std::size_t rows = C.dims().rows();
    const std::size_t cols = C.dims().cols();

    if (rows == 0 || cols == 0) {
        MatchpairsResult r;
        r.M = Value::matrix(0, 2, ValueType::DOUBLE, mr);
        r.uR = Value::matrix(rows, 1, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < rows; ++i) r.uR.doubleDataMut()[i] = i + 1;
        r.uC = Value::matrix(cols, 1, ValueType::DOUBLE, mr);
        for (std::size_t j = 0; j < cols; ++j) r.uC.doubleDataMut()[j] = j + 1;
        return r;
    }

    // Build augmented N×N matrix.
    const std::size_t N = rows + cols;
    constexpr double BIG = 1e15;
    std::vector<std::vector<double>> A(N, std::vector<double>(N, BIG));
    for (std::size_t i = 0; i < rows; ++i)
        for (std::size_t j = 0; j < cols; ++j) {
            double v = C.elemAsDouble(j * rows + i);
            if (maximise) v = -v;
            A[i][j] = v;
        }
    // Top-right block: diag(costUnmatched). Row i can opt-out via col cols+i.
    // MATLAB convention: in 'max' mode the unmatched cost ALSO flips sign
    // (it becomes a benefit/reward for leaving unmatched, so we want to
    // maximise it → minimise -costUnmatched). Matches MATLAB R2025b's
    // observed behaviour: max + high positive costUnmatched leaves
    // everything unmatched, max + zero/negative costUnmatched matches.
    const double cuSigned = maximise ? -cU : cU;
    for (std::size_t i = 0; i < rows; ++i)
        A[i][cols + i] = cuSigned;
    // Bottom-left block: diag(costUnmatched). Dummy row rows+j absorbs col j.
    for (std::size_t j = 0; j < cols; ++j)
        A[rows + j][j] = cuSigned;
    // Bottom-right block: zero (dummy-dummy free).
    for (std::size_t j = 0; j < cols; ++j)
        for (std::size_t i = 0; i < rows; ++i)
            A[rows + j][cols + i] = 0.0;

    // Jonker-Volgenant Hungarian. Indexing: 1..N internally, with row 0
    // as a sentinel (so p[0] holds the row currently being augmented).
    // Returns assignment[i] (0-based) giving column for row i.
    std::vector<double> u(N + 1, 0.0), v_d(N + 1, 0.0);
    std::vector<int> p(N + 1, 0), way(N + 1, 0);
    for (std::size_t i = 1; i <= N; ++i) {
        p[0] = static_cast<int>(i);
        int j0 = 0;
        std::vector<double> minv(N + 1, std::numeric_limits<double>::infinity());
        std::vector<char> used(N + 1, 0);
        do {
            used[j0] = 1;
            int i0 = p[j0];
            int j1 = -1;
            double delta = std::numeric_limits<double>::infinity();
            for (std::size_t j = 1; j <= N; ++j) {
                if (used[j]) continue;
                const double cur = A[i0 - 1][j - 1] - u[i0] - v_d[j];
                if (cur < minv[j]) {
                    minv[j] = cur;
                    way[j] = j0;
                }
                if (minv[j] < delta) {
                    delta = minv[j];
                    j1 = static_cast<int>(j);
                }
            }
            for (std::size_t j = 0; j <= N; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v_d[j]  -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0);
    }

    // Decode: for row i (1-based), assigned column = (1-based j where p[j] == i).
    std::vector<int> rowAssign(N, -1);
    for (std::size_t j = 1; j <= N; ++j)
        if (p[j] > 0)
            rowAssign[p[j] - 1] = static_cast<int>(j) - 1;

    // Walk real rows, build outputs.
    std::vector<std::pair<int, int>> matches;
    std::vector<int> unmatchedRows;
    matches.reserve(std::min(rows, cols));
    unmatchedRows.reserve(rows);
    for (std::size_t i = 0; i < rows; ++i) {
        const int j = rowAssign[i];
        if (j >= 0 && static_cast<std::size_t>(j) < cols)
            matches.push_back({ static_cast<int>(i) + 1, j + 1 });
        else
            unmatchedRows.push_back(static_cast<int>(i) + 1);
    }
    // Unmatched cols: those not appearing in matches.
    std::vector<char> colUsed(cols, 0);
    for (const auto &pr : matches) colUsed[pr.second - 1] = 1;
    std::vector<int> unmatchedCols;
    for (std::size_t j = 0; j < cols; ++j)
        if (!colUsed[j])
            unmatchedCols.push_back(static_cast<int>(j) + 1);

    // Pack outputs (column-major).
    MatchpairsResult result;
    result.M = Value::matrix(matches.size(), 2, ValueType::DOUBLE, mr);
    {
        double *Md = result.M.doubleDataMut();
        for (std::size_t k = 0; k < matches.size(); ++k) {
            Md[k] = matches[k].first;
            Md[matches.size() + k] = matches[k].second;
        }
    }
    result.uR = Value::matrix(unmatchedRows.size(), 1, ValueType::DOUBLE, mr);
    for (std::size_t k = 0; k < unmatchedRows.size(); ++k)
        result.uR.doubleDataMut()[k] = unmatchedRows[k];
    result.uC = Value::matrix(unmatchedCols.size(), 1, ValueType::DOUBLE, mr);
    for (std::size_t k = 0; k < unmatchedCols.size(); ++k)
        result.uC.doubleDataMut()[k] = unmatchedCols[k];
    return result;
}


} // namespace numkit::math
