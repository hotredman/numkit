// libs/image/src/region/region.cpp

#include <numkit/image/region/region.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::image {

namespace {

// Two-pass union-find labelling. Returns labels image (row-major) and
// the maximum label value (= number of components).
std::pair<std::vector<int>, int>
label_components(const std::vector<uint8_t> &fg, int H, int W, int conn)
{
    std::vector<int> L((size_t)H * (size_t)W, 0);
    std::vector<int> parent;
    parent.reserve(64);
    parent.push_back(0);  // index 0 = "background"

    auto find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    auto unite = [&](int x, int y) {
        x = find(x); y = find(y);
        if (x != y) parent[x] = y;
    };

    int next_label = 1;
    for (int r = 0; r < H; ++r) {
        for (int c = 0; c < W; ++c) {
            const size_t k = (size_t)r * (size_t)W + (size_t)c;
            if (!fg[k]) continue;
            // Look at already-labelled neighbours: N (r-1, c), W (r, c-1),
            // and for 8-connectivity also NW (r-1, c-1) and NE (r-1, c+1).
            int nb[4] = {0, 0, 0, 0};
            int nc = 0;
            if (r > 0)              { const int v = L[(size_t)(r - 1) * (size_t)W + (size_t)c];     if (v) nb[nc++] = v; }
            if (c > 0)              { const int v = L[(size_t)r * (size_t)W + (size_t)(c - 1)];     if (v) nb[nc++] = v; }
            if (conn == 8) {
                if (r > 0 && c > 0)            { const int v = L[(size_t)(r - 1) * (size_t)W + (size_t)(c - 1)]; if (v) nb[nc++] = v; }
                if (r > 0 && c + 1 < W)        { const int v = L[(size_t)(r - 1) * (size_t)W + (size_t)(c + 1)]; if (v) nb[nc++] = v; }
            }
            if (nc == 0) {
                parent.push_back(next_label);
                L[k] = next_label;
                ++next_label;
            } else {
                int m = nb[0];
                for (int i = 1; i < nc; ++i) if (nb[i] < m) m = nb[i];
                L[k] = m;
                for (int i = 0; i < nc; ++i) unite(nb[i], m);
            }
        }
    }

    // Second pass: collapse + relabel into 1..K.
    std::vector<int> remap(parent.size(), 0);
    int K = 0;
    for (size_t i = 0; i < L.size(); ++i) {
        if (L[i] == 0) continue;
        const int root = find(L[i]);
        if (remap[(size_t)root] == 0) remap[(size_t)root] = ++K;
        L[i] = remap[(size_t)root];
    }
    return {L, K};
}

std::vector<uint8_t> read_bw(const Value &BW) {
    const size_t H = BW.dims().rows();
    const size_t W = BW.dims().cols();
    std::vector<uint8_t> out(H * W);
    // Convert column-major Value into row-major flat buffer.
    for (size_t r = 0; r < H; ++r)
        for (size_t c = 0; c < W; ++c)
            out[r * W + c] = (BW.elemAsDouble(c * H + r) != 0.0) ? 1 : 0;
    return out;
}

} // anonymous

std::tuple<Value, Value>
bwlabel(std::pmr::memory_resource *mr, const Value &BW, int conn)
{
    if (conn != 4) conn = 8;
    const int H = (int)BW.dims().rows();
    const int W = (int)BW.dims().cols();
    auto [L, K] = label_components(read_bw(BW), H, W, conn);
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r)
            od[(size_t)c * (size_t)H + (size_t)r]
                = double(L[(size_t)r * (size_t)W + (size_t)c]);
    return std::make_tuple(std::move(out), Value::scalar(double(K), mr));
}

std::tuple<Value, Value, Value, Value>
bwconncomp(std::pmr::memory_resource *mr, const Value &BW, int conn)
{
    if (conn != 4) conn = 8;
    const int H = (int)BW.dims().rows();
    const int W = (int)BW.dims().cols();
    auto [L, K] = label_components(read_bw(BW), H, W, conn);

    // ImageSize = [H, W].
    Value sz = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    sz.doubleDataMut()[0] = double(H);
    sz.doubleDataMut()[1] = double(W);

    // PixelIdxList: cell array of column vectors of linear indices
    // (1-based, column-major to match MATLAB).
    // Without easy cell construction here, we return a single matrix
    // (max-component-size) × K, padded with NaN. Callers can post-process.
    if (K == 0) {
        return std::make_tuple(Value::scalar(double(conn), mr),
                               std::move(sz),
                               Value::scalar(0.0, mr),
                               Value::matrix(0, 0, ValueType::DOUBLE, mr));
    }
    std::vector<std::vector<int>> lists((size_t)K);
    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const int lab = L[(size_t)r * (size_t)W + (size_t)c];
            if (lab > 0)
                lists[(size_t)lab - 1].push_back(c * H + r + 1);  // 1-based linear
        }
    size_t maxlen = 0;
    for (const auto &v : lists) if (v.size() > maxlen) maxlen = v.size();
    Value pix = Value::matrix(maxlen, (size_t)K, ValueType::DOUBLE, mr);
    double *pd = pix.doubleDataMut();
    for (size_t k = 0; k < (size_t)K; ++k)
        for (size_t i = 0; i < maxlen; ++i)
            pd[k * maxlen + i] = (i < lists[k].size()) ? double(lists[k][i])
                                                       : std::nan("");
    return std::make_tuple(Value::scalar(double(conn), mr),
                           std::move(sz),
                           Value::scalar(double(K), mr),
                           std::move(pix));
}

Value bwarea(std::pmr::memory_resource *mr, const Value &BW) {
    const size_t N = BW.numel();
    double area = 0.0;
    for (size_t i = 0; i < N; ++i)
        if (BW.elemAsDouble(i) != 0.0) area += 1.0;
    return Value::scalar(area, mr);
}

Value bwperim(std::pmr::memory_resource *mr, const Value &BW, int conn) {
    if (conn != 4) conn = 8;
    const int H = (int)BW.dims().rows();
    const int W = (int)BW.dims().cols();
    auto fg = read_bw(BW);
    Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;
    uint8_t *od = out.logicalDataMut();
    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const size_t k = (size_t)r * (size_t)W + (size_t)c;
            if (!fg[k]) { od[(size_t)c * (size_t)H + (size_t)r] = 0; continue; }
            bool boundary = false;
            for (int dr = -1; dr <= 1 && !boundary; ++dr)
                for (int dc = -1; dc <= 1 && !boundary; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    if (conn == 4 && std::abs(dr) + std::abs(dc) > 1) continue;
                    const int rr = r + dr, cc = c + dc;
                    if (rr < 0 || rr >= H || cc < 0 || cc >= W) {
                        boundary = true; break;
                    }
                    if (!fg[(size_t)rr * (size_t)W + (size_t)cc]) boundary = true;
                }
            od[(size_t)c * (size_t)H + (size_t)r] = boundary ? 1 : 0;
        }
    return out;
}

Value bwareaopen(std::pmr::memory_resource *mr, const Value &BW, int P, int conn)
{
    if (conn != 4) conn = 8;
    if (P < 1) P = 1;
    const int H = (int)BW.dims().rows();
    const int W = (int)BW.dims().cols();
    auto [L, K] = label_components(read_bw(BW), H, W, conn);
    std::vector<int> count((size_t)K + 1, 0);
    for (int v : L) if (v > 0) ++count[(size_t)v];

    Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;
    uint8_t *od = out.logicalDataMut();
    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const int lab = L[(size_t)r * (size_t)W + (size_t)c];
            const bool keep = (lab > 0) && (count[(size_t)lab] >= P);
            od[(size_t)c * (size_t)H + (size_t)r] = keep ? 1 : 0;
        }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// bwboundaries — Moore-neighbour outer-boundary trace
// ════════════════════════════════════════════════════════════════════
//
// For each connected component (labelled via the same union-find
// pass we already use for bwlabel/bwperim/bwareaopen), find the
// leftmost-topmost foreground pixel of that component and walk the
// outer boundary clockwise. We use Moore-neighbour tracing with
// Jacob's stopping criterion: stop when we revisit the start pixel
// from the same direction we left it.
//
// Boundary points are emitted as (row, col) pairs in MATLAB 1-based
// indexing. Inner holes are NOT traced (matches `'noholes'` mode);
// outer-only is what most scripts want and we'd need a second pass
// to gather hole boundaries from the complement.

Value bwboundaries(std::pmr::memory_resource *mr,
                   const Value &BW, int conn)
{
    if (conn != 4) conn = 8;
    const int H = (int)BW.dims().rows();
    const int W = (int)BW.dims().cols();
    auto fg = read_bw(BW);
    auto [L, K] = label_components(fg, H, W, conn);
    Value cellCol = Value::cell(static_cast<size_t>(K), 1, mr);
    if (K == 0) return cellCol;

    // 8-direction CW deltas starting at "right" (dir=0):
    //   E, SE, S, SW, W, NW, N, NE.
    static const int dr[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int dc[8] = { 1, 1, 0,-1,-1, -1,  0,  1 };
    auto inside = [&](int r, int c) {
        return r >= 0 && r < H && c >= 0 && c < W;
    };
    auto isFg = [&](int r, int c, int targetLabel) {
        if (!inside(r, c)) return false;
        return L[(size_t)r * (size_t)W + (size_t)c] == targetLabel;
    };

    // Start-pixel scan: first encounter of each label in row-major
    // order is its leftmost-topmost pixel. Cache that lookup.
    std::vector<int> startRow(K + 1, -1), startCol(K + 1, -1);
    for (int r = 0; r < H && std::count(startRow.begin(),
                                         startRow.end(), -1) > 1; ++r)
        for (int c = 0; c < W; ++c) {
            const int lab = L[(size_t)r * (size_t)W + (size_t)c];
            if (lab > 0 && startRow[lab] < 0) {
                startRow[lab] = r;
                startCol[lab] = c;
            }
        }

    for (int lab = 1; lab <= K; ++lab) {
        const int r0 = startRow[lab];
        const int c0 = startCol[lab];
        std::vector<int> rows, cols;
        rows.push_back(r0); cols.push_back(c0);

        // Single-pixel object: emit just the start point.
        bool hasNeighbour = false;
        for (int d = 0; d < 8; ++d)
            if (isFg(r0 + dr[d], c0 + dc[d], lab)) {
                hasNeighbour = true; break;
            }
        if (!hasNeighbour) {
            // Cell entry: 1×2 [row col] (1-based).
            Value m = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            double *mb = m.doubleDataMut();
            mb[0] = double(r0 + 1);
            mb[1] = double(c0 + 1);
            cellCol.cellAt(static_cast<size_t>(lab - 1)) = m;
            continue;
        }

        // The previous-direction starts at the "from" we'd have used
        // if we entered the start from the west (4-conn) / NW (8-conn).
        int prevDir = (conn == 8) ? 7 : 6;   // NW or N
        int curR = r0, curC = c0;

        // Cap iteration at perimeter ≤ 8·numComponentPixels (very loose).
        const size_t cap = static_cast<size_t>(H) * static_cast<size_t>(W) * 4;
        size_t iter = 0;

        while (iter++ < cap) {
            // Look at the 8 neighbours starting one CW step past `prevDir`.
            int found = -1;
            for (int step = 1; step <= 8; ++step) {
                const int d = (prevDir + step) % 8;
                if (conn == 4 && (d & 1)) continue;   // skip diagonals
                const int nr = curR + dr[d];
                const int nc = curC + dc[d];
                if (isFg(nr, nc, lab)) { found = d; break; }
            }
            if (found < 0) break;   // isolated pixel reached
            const int nr = curR + dr[found];
            const int nc = curC + dc[found];
            // The direction we came FROM the new pixel is opposite of
            // the one we just took.
            prevDir = (found + 4) % 8;
            // Jacob's stop: back at start *and* would re-enter via the
            // same direction we left.
            if (nr == r0 && nc == c0) {
                // Emit closing entry.
                rows.push_back(r0);
                cols.push_back(c0);
                break;
            }
            rows.push_back(nr);
            cols.push_back(nc);
            curR = nr;
            curC = nc;
        }

        // Pack into a P×2 [row col] (1-based).
        const size_t P = rows.size();
        Value m = Value::matrix(P, 2, ValueType::DOUBLE, mr);
        double *mb = m.doubleDataMut();
        for (size_t i = 0; i < P; ++i) {
            mb[i]         = double(rows[i] + 1);
            mb[P + i]     = double(cols[i] + 1);
        }
        cellCol.cellAt(static_cast<size_t>(lab - 1)) = m;
    }
    return cellCol;
}

// ════════════════════════════════════════════════════════════════════
// regionprops — basic descriptors per labelled region
// ════════════════════════════════════════════════════════════════════

Value regionprops(std::pmr::memory_resource *mr,
                  const Value &BW_or_L,
                  const std::vector<std::string> &propsIn)
{
    // Detect input: integer-valued matrix → treat as label image,
    // else binary → run bwlabel internally.
    const int H = (int)BW_or_L.dims().rows();
    const int W = (int)BW_or_L.dims().cols();
    std::vector<int> L((size_t)H * (size_t)W, 0);
    int K = 0;
    bool looksLabelled = false;
    {
        // Quick scan: are any values > 1 with integer fractional part?
        const size_t N = (size_t)H * (size_t)W;
        for (size_t i = 0; i < N; ++i) {
            const double v = BW_or_L.elemAsDouble(i);
            if (v > 1.5) { looksLabelled = true; break; }
        }
    }
    if (looksLabelled) {
        // Treat as label image directly; copy in row-major.
        for (int r = 0; r < H; ++r)
            for (int c = 0; c < W; ++c) {
                const int v = (int)std::round(
                    BW_or_L.elemAsDouble((size_t)c * (size_t)H + (size_t)r));
                L[(size_t)r * (size_t)W + (size_t)c] = v;
                if (v > K) K = v;
            }
    } else {
        auto [labels, kk] = label_components(read_bw(BW_or_L), H, W, 8);
        L = std::move(labels);
        K = kk;
    }

    // Property selection.
    auto contains = [&](const char *p) {
        for (const auto &q : propsIn) {
            if (q == p) return true;
            // Case-insensitive ASCII compare.
            if (q.size() != std::strlen(p)) continue;
            bool eq = true;
            for (size_t i = 0; i < q.size(); ++i) {
                char a = q[i], b = p[i];
                if (a >= 'A' && a <= 'Z') a = char(a + 32);
                if (b >= 'A' && b <= 'Z') b = char(b + 32);
                if (a != b) { eq = false; break; }
            }
            if (eq) return true;
        }
        return false;
    };
    bool wantAll = propsIn.empty() || contains("all") || contains("All");
    const bool wArea = wantAll || contains("Area");
    const bool wCent = wantAll || contains("Centroid");
    const bool wBbox = wantAll || contains("BoundingBox");

    Value sa = Value::structArray(static_cast<size_t>(K), 1, mr);
    if (K == 0) return sa;

    // Accumulators per label.
    std::vector<long long> area(K + 1, 0);
    std::vector<double> sumX(K + 1, 0.0), sumY(K + 1, 0.0);
    std::vector<int> minX(K + 1, INT_MAX), minY(K + 1, INT_MAX);
    std::vector<int> maxX(K + 1, INT_MIN), maxY(K + 1, INT_MIN);
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            const int lab = L[(size_t)r * (size_t)W + (size_t)c];
            if (lab <= 0 || lab > K) continue;
            ++area[(size_t)lab];
            sumX[(size_t)lab] += double(c);
            sumY[(size_t)lab] += double(r);
            if (c < minX[(size_t)lab]) minX[(size_t)lab] = c;
            if (r < minY[(size_t)lab]) minY[(size_t)lab] = r;
            if (c > maxX[(size_t)lab]) maxX[(size_t)lab] = c;
            if (r > maxY[(size_t)lab]) maxY[(size_t)lab] = r;
        }

    for (int lab = 1; lab <= K; ++lab) {
        auto &el = sa.structArrayElem(static_cast<size_t>(lab - 1));
        if (wArea) {
            el.emplace("Area", Value::scalar(double(area[(size_t)lab]), mr));
        }
        if (wCent) {
            // Centroid in MATLAB: 1-based (x, y) = (col+1, row+1) means.
            const double a = double(area[(size_t)lab]);
            Value cn = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            cn.doubleDataMut()[0] = (a > 0.0) ? sumX[(size_t)lab] / a + 1.0 : 0.0;
            cn.doubleDataMut()[1] = (a > 0.0) ? sumY[(size_t)lab] / a + 1.0 : 0.0;
            el.emplace("Centroid", cn);
        }
        if (wBbox) {
            // BoundingBox: [xmin-0.5, ymin-0.5, width, height]
            // (MATLAB returns 0.5-aligned floats).
            Value bb = Value::matrix(1, 4, ValueType::DOUBLE, mr);
            bb.doubleDataMut()[0] = double(minX[(size_t)lab]) + 0.5;
            bb.doubleDataMut()[1] = double(minY[(size_t)lab]) + 0.5;
            bb.doubleDataMut()[2] = double(maxX[(size_t)lab] -
                                            minX[(size_t)lab] + 1);
            bb.doubleDataMut()[3] = double(maxY[(size_t)lab] -
                                            minY[(size_t)lab] + 1);
            el.emplace("BoundingBox", bb);
        }
    }
    return sa;
}

// ════════════════════════════════════════════════════════════════════
// bwdist — Euclidean distance transform
// ════════════════════════════════════════════════════════════════════
//
// Felzenszwalb-Huttenlocher (2004) 1-D parabolic-envelope distance
// transform applied row-wise then column-wise. Exact, O(H·W).
//
// The 1-D DT computes  D(x) = min_y { (x − y)² + f(y) }  in O(N).
// We use a sentinel f = +Inf for background pixels and 0 for
// foreground; running the 1-D DT along each row first squashes f
// horizontally, then the column pass combines that into the full
// 2-D Euclidean DT (proof: separability of squared distance in the
// max-of-min formulation).

namespace {

void dt1D(const std::vector<double> &f, std::vector<double> &d, size_t n)
{
    // v[k] is the index of the k-th parabola in the lower envelope;
    // z[k] is the boundary between parabolas k-1 and k.
    std::vector<long long> v(n);
    std::vector<double> z(n + 1);
    long long k = 0;
    v[0] = 0;
    z[0] = -std::numeric_limits<double>::infinity();
    z[1] =  std::numeric_limits<double>::infinity();
    for (long long q = 1; q < static_cast<long long>(n); ++q) {
        // Skip background pixels in f (they contribute Inf — no parabola).
        // Keep them anyway: their +Inf value pushes any candidate boundary
        // away to +Inf. That's fine.
        double s;
        for (;;) {
            const double fq = f[static_cast<size_t>(q)];
            const double fv = f[static_cast<size_t>(v[k])];
            const double dq = static_cast<double>(q);
            const double dv = static_cast<double>(v[k]);
            if (std::isinf(fq) && std::isinf(fv)) {
                // Both background — boundary is at -Inf, parabola q
                // dominates from k onward.
                s = -std::numeric_limits<double>::infinity();
            } else {
                s = ((fq + dq * dq) - (fv + dv * dv)) / (2.0 * (dq - dv));
            }
            if (s <= z[static_cast<size_t>(k)]) {
                --k;
                if (k < 0) break;
            } else break;
        }
        ++k;
        v[static_cast<size_t>(k)] = q;
        z[static_cast<size_t>(k)] = s;
        z[static_cast<size_t>(k) + 1] = std::numeric_limits<double>::infinity();
    }
    long long kk = 0;
    for (long long q = 0; q < static_cast<long long>(n); ++q) {
        while (z[static_cast<size_t>(kk) + 1] < static_cast<double>(q)) ++kk;
        const double dx = static_cast<double>(q - v[static_cast<size_t>(kk)]);
        d[static_cast<size_t>(q)] = dx * dx +
            f[static_cast<size_t>(v[static_cast<size_t>(kk)])];
    }
}

} // anonymous

Value bwdist(std::pmr::memory_resource *mr, const Value &BW)
{
    const size_t H = BW.dims().rows();
    const size_t W = BW.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;

    const double INF = std::numeric_limits<double>::infinity();
    // Pull BW into a flat row-major foreground/background buffer.
    std::vector<double> f(H * W, 0.0);
    for (size_t r = 0; r < H; ++r)
        for (size_t c = 0; c < W; ++c) {
            // numkit storage is column-major: idx = c*H + r.
            const double v = BW.elemAsDouble(c * H + r);
            f[r * W + c] = (v != 0.0) ? 0.0 : INF;
        }

    // Pass 1: distance transform along each row (horizontal DT).
    std::vector<double> rowIn(W), rowOut(W);
    std::vector<double> g(H * W, 0.0);
    for (size_t r = 0; r < H; ++r) {
        for (size_t c = 0; c < W; ++c) rowIn[c] = f[r * W + c];
        dt1D(rowIn, rowOut, W);
        for (size_t c = 0; c < W; ++c) g[r * W + c] = rowOut[c];
    }

    // Pass 2: distance transform along each column (vertical DT)
    // operating on the squared-distance result of pass 1.
    std::vector<double> colIn(H), colOut(H);
    for (size_t c = 0; c < W; ++c) {
        for (size_t r = 0; r < H; ++r) colIn[r] = g[r * W + c];
        dt1D(colIn, colOut, H);
        for (size_t r = 0; r < H; ++r) {
            const double sq = colOut[r];
            // Take sqrt to get true Euclidean distance; +Inf survives.
            const double d = std::isinf(sq) ? INF : std::sqrt(sq);
            out.doubleDataMut()[c * H + r] = d;
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void bwlabel_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwlabel: requires (BW[, conn])", 0, 0, "bwlabel", "",
                    "m:bwlabel:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    auto [L, n] = bwlabel(ctx.engine->resource(), args[0], conn);
    outs[0] = std::move(L);
    if (nargout > 1) outs[1] = std::move(n);
}

void bwconncomp_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwconncomp: requires (BW[, conn])", 0, 0, "bwconncomp", "",
                    "m:bwconncomp:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    auto [c, sz, n, p] = bwconncomp(ctx.engine->resource(), args[0], conn);
    outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(sz);
    if (nargout > 2) outs[2] = std::move(n);
    if (nargout > 3) outs[3] = std::move(p);
}

void bwarea_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwarea: requires BW", 0, 0, "bwarea", "",
                    "m:bwarea:nargin");
    outs[0] = bwarea(ctx.engine->resource(), args[0]);
}

void bwperim_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwperim: requires (BW[, conn])", 0, 0, "bwperim", "",
                    "m:bwperim:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    outs[0] = bwperim(ctx.engine->resource(), args[0], conn);
}

void bwareaopen_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwareaopen: requires (BW, P[, conn])", 0, 0,
                    "bwareaopen", "", "m:bwareaopen:nargin");
    const int P = (int)args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? (int)args[2].toScalar() : 8;
    outs[0] = bwareaopen(ctx.engine->resource(), args[0], P, conn);
}

void bwboundaries_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwboundaries: requires (BW[, conn])", 0, 0,
                    "bwboundaries", "", "m:bwboundaries:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    outs[0] = bwboundaries(ctx.engine->resource(), args[0], conn);
}

void regionprops_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("regionprops: requires (BW_or_L[, props...])",
                    0, 0, "regionprops", "", "m:regionprops:nargin");
    std::vector<std::string> props;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("regionprops: property names must be strings",
                        0, 0, "regionprops", "", "m:regionprops:type");
        props.push_back(args[i].toString());
    }
    outs[0] = regionprops(ctx.engine->resource(), args[0], props);
}

void bwdist_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwdist: requires (BW)",
                    0, 0, "bwdist", "", "m:bwdist:nargin");
    outs[0] = bwdist(ctx.engine->resource(), args[0]);
}

} // namespace detail
} // namespace numkit::image
