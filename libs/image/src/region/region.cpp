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
    // MATLAB-compatible column-major scan order — labels are assigned
    // in the order each new component is first encountered in
    // column-major (col 0 row 0..H-1, col 1 row 0..H-1, ...) order.
    // Already-labelled neighbours at this scan point are: W (r, c-1),
    // N (r-1, c), and for 8-conn also NW (r-1, c-1) and SW (r+1, c-1).
    for (int c = 0; c < W; ++c) {
        for (int r = 0; r < H; ++r) {
            const size_t k = (size_t)r * (size_t)W + (size_t)c;
            if (!fg[k]) continue;
            int nb[4] = {0, 0, 0, 0};
            int nc = 0;
            if (c > 0)              { const int v = L[(size_t)r * (size_t)W + (size_t)(c - 1)];     if (v) nb[nc++] = v; }
            if (r > 0)              { const int v = L[(size_t)(r - 1) * (size_t)W + (size_t)c];     if (v) nb[nc++] = v; }
            if (conn == 8) {
                if (c > 0 && r > 0)            { const int v = L[(size_t)(r - 1) * (size_t)W + (size_t)(c - 1)]; if (v) nb[nc++] = v; }
                if (c > 0 && r + 1 < H)        { const int v = L[(size_t)(r + 1) * (size_t)W + (size_t)(c - 1)]; if (v) nb[nc++] = v; }
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

    // Second pass: collapse + relabel into 1..K in COLUMN-MAJOR
    // scan order (so labels match MATLAB's first-pixel-encountered
    // convention).
    std::vector<int> remap(parent.size(), 0);
    int K = 0;
    for (int c = 0; c < W; ++c) {
        for (int r = 0; r < H; ++r) {
            const size_t i = (size_t)r * (size_t)W + (size_t)c;
            if (L[i] == 0) continue;
            const int root = find(L[i]);
            if (remap[(size_t)root] == 0) remap[(size_t)root] = ++K;
            L[i] = remap[(size_t)root];
        }
    }
    // Apply remap to ALL pixels (in case any were missed by the
    // column-major walk above — none should be, but defensive).
    for (size_t i = 0; i < L.size(); ++i) {
        if (L[i] == 0) continue;
        const int root = find(L[i]);
        if (remap[(size_t)root] != 0) L[i] = remap[(size_t)root];
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
bwlabel(const Value &BW, int conn, std::pmr::memory_resource *mr)
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

Value bwconncomp(const Value &BW, int conn, std::pmr::memory_resource *mr)
{
    if (conn != 4) conn = 8;
    const int H = (int)BW.dims().rows();
    const int W = (int)BW.dims().cols();
    auto [L, K] = label_components(read_bw(BW), H, W, conn);

    // MATLAB convention: bwconncomp returns a 1x1 struct with fields
    //   Connectivity (scalar)
    //   ImageSize    ([H, W])
    //   NumObjects   (scalar)
    //   PixelIdxList (1xK cell of column-vector 1-based linear indices,
    //                 column-major).
    auto cc = Value::structArray(1, 1, mr);
    auto &el = cc.structArrayElem(0);

    el.emplace("Connectivity", Value::scalar(double(conn), mr));

    Value sz = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    sz.doubleDataMut()[0] = double(H);
    sz.doubleDataMut()[1] = double(W);
    el.emplace("ImageSize", std::move(sz));

    el.emplace("NumObjects", Value::scalar(double(K), mr));

    // Build PixelIdxList: 1xK cell of column-vector indices.
    Value pixList = Value::cell(1, static_cast<size_t>(K), mr);
    if (K > 0) {
        std::vector<std::vector<int>> lists((size_t)K);
        for (int c = 0; c < W; ++c)
            for (int r = 0; r < H; ++r) {
                const int lab = L[(size_t)r * (size_t)W + (size_t)c];
                if (lab > 0)
                    lists[(size_t)lab - 1].push_back(c * H + r + 1);  // 1-based linear
            }
        for (size_t k = 0; k < (size_t)K; ++k) {
            const auto &v = lists[k];
            Value col = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
            double *cd = col.doubleDataMut();
            for (size_t i = 0; i < v.size(); ++i) cd[i] = double(v[i]);
            pixList.cellAt(k) = std::move(col);
        }
    }
    el.emplace("PixelIdxList", std::move(pixList));

    return cc;
}

// ────────────────────────────────────────────────────────────────────
// labelmatrix / cc2bw — CC struct conversions
// ────────────────────────────────────────────────────────────────────
//
// Both functions accept a struct produced by bwconncomp and rasterise
// its PixelIdxList cells back into the image grid. Algorithms are
// straightforward direct transliterations of MATLAB R2025b
// labelmatrix.m / cc2bw.m.

namespace {

// Common helper: extract (H, W) image size and PixelIdxList from a CC
// struct. Validates field presence + types.
struct CCInfo {
    std::size_t H = 0, W = 0;
    std::size_t K = 0;             // NumObjects
    const Value *pixList = nullptr;  // 1×K cell of column-vector indices
};

CCInfo parse_cc(const Value &CC, const char *fn)
{
    CCInfo info;
    if (!CC.isStruct() || CC.numel() != 1)
        throw Error(std::string(fn) + ": CC must be a 1×1 struct from "
                    "bwconncomp",
                    0, 0, fn, "", std::string("numkit:") + fn + ":notStruct");
    const auto &el = CC.structArrayElem(0);
    auto need = [&](const char *name) -> const Value & {
        auto it = el.find(name);
        if (it == el.end())
            throw Error(std::string(fn) + ": CC missing field '"
                        + name + "'", 0, 0, fn, "",
                        std::string("numkit:") + fn + ":noField");
        return it->second;
    };
    const Value &sz = need("ImageSize");
    if (sz.numel() < 2)
        throw Error(std::string(fn) + ": CC.ImageSize must have >= 2 dims",
                    0, 0, fn, "",
                    std::string("numkit:") + fn + ":sizeDim");
    info.H = static_cast<std::size_t>(sz.elemAsDouble(0));
    info.W = static_cast<std::size_t>(sz.elemAsDouble(1));
    const Value &nob = need("NumObjects");
    info.K = static_cast<std::size_t>(nob.toScalar());
    const Value &pl = need("PixelIdxList");
    info.pixList = &pl;
    return info;
}

}  // namespace

Value labelmatrix(const Value &CC, std::pmr::memory_resource *mr)
{
    const CCInfo info = parse_cc(CC, "labelmatrix");
    const std::size_t H = info.H, W = info.W, K = info.K;
    // Choose output class per MATLAB:
    //   K ≤ 255      → uint8
    //   K ≤ 65535    → uint16
    //   K ≤ 2³² - 1  → uint32
    //   else         → double
    ValueType ot;
    if      (K <= 255)        ot = ValueType::UINT8;
    else if (K <= 65535)      ot = ValueType::UINT16;
    else if (K <= 0xFFFFFFFFULL) ot = ValueType::UINT32;
    else                       ot = ValueType::DOUBLE;
    Value L = Value::matrix(H, W, ot, mr);
    // Zero-fill is already done by matrix constructor; defensive memset
    // is unnecessary.
    auto write_label = [&](std::size_t idx0, std::size_t lab) {
        // idx0: 0-based linear index in column-major H×W.
        if (idx0 >= H * W) return;
        switch (ot) {
            case ValueType::UINT8:  L.uint8DataMut()[idx0]
                = static_cast<uint8_t>(lab);  break;
            case ValueType::UINT16: L.uint16DataMut()[idx0]
                = static_cast<uint16_t>(lab); break;
            case ValueType::UINT32: L.uint32DataMut()[idx0]
                = static_cast<uint32_t>(lab); break;
            default:                L.doubleDataMut()[idx0]
                = static_cast<double>(lab);   break;
        }
    };
    for (std::size_t k = 0; k < K; ++k) {
        const Value &cell = info.pixList->cellAt(k);
        const std::size_t N = cell.numel();
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t idx1 = static_cast<std::size_t>(
                cell.elemAsDouble(i));   // 1-based
            if (idx1 == 0) continue;
            write_label(idx1 - 1, k + 1);
        }
    }
    return L;
}

Value cc2bw(const Value &CC, const Value &objects_to_keep,
            std::pmr::memory_resource *mr)
{
    const CCInfo info = parse_cc(CC, "cc2bw");
    const std::size_t H = info.H, W = info.W, K = info.K;

    // Resolve which components to rasterise.
    std::vector<char> keep(K, 0);
    if (objects_to_keep.isEmpty()) {
        std::fill(keep.begin(), keep.end(), 1);
    } else {
        const std::size_t N = objects_to_keep.numel();
        if (objects_to_keep.type() == ValueType::LOGICAL) {
            // Logical mask: length must equal K.
            if (N != K)
                throw Error("cc2bw: ObjectsToKeep logical vector length "
                            "must equal NumObjects",
                            0, 0, "cc2bw", "",
                            "numkit:cc2bw:logicalLen");
            for (std::size_t i = 0; i < K; ++i)
                keep[i] = objects_to_keep.elemAsDouble(i) != 0.0 ? 1 : 0;
        } else {
            // Numeric indices — must be positive integers ≤ K.
            for (std::size_t i = 0; i < N; ++i) {
                const double v = objects_to_keep.elemAsDouble(i);
                if (!std::isfinite(v) || v <= 0.0 || std::floor(v) != v)
                    throw Error("cc2bw: ObjectsToKeep must be positive "
                                "integers or a logical vector",
                                0, 0, "cc2bw", "",
                                "numkit:cc2bw:badIdx");
                const std::size_t idx = static_cast<std::size_t>(v);
                if (idx > K)
                    throw Error("cc2bw: ObjectsToKeep index exceeds "
                                "NumObjects",
                                0, 0, "cc2bw", "",
                                "numkit:cc2bw:idxRange");
                keep[idx - 1] = 1;
            }
        }
    }

    Value BW = Value::matrix(H, W, ValueType::LOGICAL, mr);
    uint8_t *bd = BW.logicalDataMut();
    std::fill(bd, bd + H * W, static_cast<uint8_t>(0));
    for (std::size_t k = 0; k < K; ++k) {
        if (!keep[k]) continue;
        const Value &cell = info.pixList->cellAt(k);
        const std::size_t N = cell.numel();
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t idx1 = static_cast<std::size_t>(
                cell.elemAsDouble(i));
            if (idx1 == 0 || idx1 > H * W) continue;
            bd[idx1 - 1] = 1;
        }
    }
    return BW;
}

Value bwarea(const Value &BW, std::pmr::memory_resource *mr) {
    const size_t N = BW.numel();
    double area = 0.0;
    for (size_t i = 0; i < N; ++i)
        if (BW.elemAsDouble(i) != 0.0) area += 1.0;
    return Value::scalar(area, mr);
}

Value bwperim(const Value &BW, int conn, std::pmr::memory_resource *mr) {
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

Value bwareaopen(const Value &BW, int P, int conn, std::pmr::memory_resource *mr)
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

Value bwboundaries(const Value &BW, int conn, std::pmr::memory_resource *mr)
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

Value regionprops(const Value &BW_or_L, const std::vector<std::string> &propsIn, std::pmr::memory_resource *mr)
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

Value bwdist(const Value &BW, std::pmr::memory_resource *mr)
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

Value roicolor(const Value &A, const Value &low_or_v, double high, bool is_range, std::pmr::memory_resource *mr)
{
    const auto &d = A.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1;
    Value out = d.is3D()
        ? Value::matrix3d(H, W, P, ValueType::LOGICAL, mr)
        : Value::matrix(H, W, ValueType::LOGICAL, mr);
    const size_t N = A.numel();
    if (N == 0) return out;
    std::uint8_t *od = out.logicalDataMut();

    if (is_range) {
        const double lo = low_or_v.toScalar();
        const double hi = high;
        for (size_t i = 0; i < N; ++i) {
            const double v = A.elemAsDouble(i);
            od[i] = (v >= lo && v <= hi) ? 1u : 0u;
        }
    } else {
        const size_t M = low_or_v.numel();
        for (size_t i = 0; i < N; ++i) {
            const double v = A.elemAsDouble(i);
            std::uint8_t b = 0;
            for (size_t k = 0; k < M; ++k)
                if (v == low_or_v.elemAsDouble(k)) { b = 1; break; }
            od[i] = b;
        }
    }
    return out;
}

Value fchcode(const Value &bound, std::pmr::memory_resource *mr)
{
    if (bound.dims().cols() != 2)
        throw Error("fchcode: bound must be K-by-2",
                    0, 0, "fchcode", "", "numkit:fchcode:size");
    size_t K = bound.dims().rows();

    // Direction map matching Octave (rows = dr+2, cols = dc+2):
    //   dr=-1: dc=-1→3, dc=0→2, dc=+1→1
    //   dr= 0: dc=-1→4, dc=0→NaN, dc=+1→0
    //   dr=+1: dc=-1→5, dc=0→6, dc=+1→7
    auto dirCode = [](int dr, int dc) -> double {
        if (dr == -1 && dc == -1) return 3.0;
        if (dr == -1 && dc ==  0) return 2.0;
        if (dr == -1 && dc ==  1) return 1.0;
        if (dr ==  0 && dc == -1) return 4.0;
        if (dr ==  0 && dc ==  0) return std::nan("");
        if (dr ==  0 && dc ==  1) return 0.0;
        if (dr ==  1 && dc == -1) return 5.0;
        if (dr ==  1 && dc ==  0) return 6.0;
        if (dr ==  1 && dc ==  1) return 7.0;
        return std::nan("");
    };

    // Read bound rows into vectors r, c.
    std::vector<double> rs(K), cs(K);
    for (size_t k = 0; k < K; ++k) {
        // Column-major: bound[k, 0] = data[0*K + k]; bound[k, 1] = data[1*K + k].
        rs[k] = bound.elemAsDouble(0 * K + k);
        cs[k] = bound.elemAsDouble(1 * K + k);
    }

    // Close the loop if not already closed.
    if (K >= 1 && (rs.front() != rs.back() || cs.front() != cs.back())) {
        rs.push_back(rs.front());
        cs.push_back(cs.front());
        K = rs.size();
    }
    const size_t nCodes = (K >= 1) ? K - 1 : 0;

    Value out = Value::structure(mr);

    // x0y0
    Value xy0 = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    if (K >= 1) {
        xy0.doubleDataMut()[0] = rs.empty() ? 0.0 : rs[0];
        xy0.doubleDataMut()[1] = cs.empty() ? 0.0 : cs[0];
    }
    out.field("x0y0") = xy0;

    // fcc — 1×nCodes.
    Value fcc = Value::matrix(1, nCodes, ValueType::DOUBLE, mr);
    double *fd = fcc.doubleDataMut();
    for (size_t k = 0; k < nCodes; ++k) {
        const int dr = static_cast<int>(rs[k + 1] - rs[k]);
        const int dc = static_cast<int>(cs[k + 1] - cs[k]);
        fd[k] = dirCode(dr, dc);
    }
    out.field("fcc") = fcc;

    // diff — mod 8 first-difference, cyclic.
    Value diffV = Value::matrix(1, nCodes, ValueType::DOUBLE, mr);
    double *dd = diffV.doubleDataMut();
    for (size_t k = 0; k < nCodes; ++k) {
        const double a = fd[k];
        const double b = fd[(k + 1) % nCodes];
        const double d = b - a;
        // mod-8 (positive remainder).
        double m = std::fmod(d, 8.0);
        if (m < 0.0) m += 8.0;
        dd[k] = m;
    }
    out.field("diff") = diffV;

    return out;
}

Value bwareafilt(const Value &BW, double lo, double hi, size_t n_keep, bool keep_largest, int conn, std::pmr::memory_resource *mr)
{
    if (conn != 4) conn = 8;
    const int H = static_cast<int>(BW.dims().rows());
    const int W = static_cast<int>(BW.dims().cols());

    Value out = Value::matrix(static_cast<size_t>(H),
                              static_cast<size_t>(W),
                              ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;

    auto [L, K] = label_components(read_bw(BW), H, W, conn);
    if (K == 0) return out;

    // Compute area per label (1-based labels in L).
    std::vector<size_t> area(static_cast<size_t>(K) + 1, 0);
    for (size_t i = 0; i < L.size(); ++i) {
        const int k = L[i];
        if (k > 0) ++area[static_cast<size_t>(k)];
    }

    // Decide kept labels.
    std::vector<uint8_t> keep(static_cast<size_t>(K) + 1, 0);
    if (n_keep > 0) {
        // Top-N form. Sort labels [1..K] by area, ties by lower index.
        std::vector<int> idx;
        idx.reserve(static_cast<size_t>(K));
        for (int k = 1; k <= K; ++k) idx.push_back(k);
        if (keep_largest) {
            std::sort(idx.begin(), idx.end(), [&](int a, int b) {
                if (area[(size_t)a] != area[(size_t)b])
                    return area[(size_t)a] > area[(size_t)b];
                return a < b;
            });
        } else {
            std::sort(idx.begin(), idx.end(), [&](int a, int b) {
                if (area[(size_t)a] != area[(size_t)b])
                    return area[(size_t)a] < area[(size_t)b];
                return a < b;
            });
        }
        const size_t take = std::min(n_keep, idx.size());
        for (size_t i = 0; i < take; ++i)
            keep[static_cast<size_t>(idx[i])] = 1;
    } else {
        // Range form.
        for (int k = 1; k <= K; ++k) {
            const double a = static_cast<double>(area[(size_t)k]);
            keep[(size_t)k] = (a >= lo && a <= hi) ? 1u : 0u;
        }
    }

    // Write output (col-major). L is row-major.
    uint8_t *od = out.logicalDataMut();
    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const int k = L[(size_t)r * (size_t)W + (size_t)c];
            od[(size_t)c * (size_t)H + (size_t)r] =
                (k > 0 && keep[(size_t)k]) ? 1u : 0u;
        }
    return out;
}

std::tuple<Value, Value>
bwselect(const Value &BW, const Value &cols, const Value &rows, int conn, std::pmr::memory_resource *mr)
{
    if (conn != 4) conn = 8;
    const int H = static_cast<int>(BW.dims().rows());
    const int W = static_cast<int>(BW.dims().cols());

    Value out = Value::matrix(static_cast<size_t>(H),
                              static_cast<size_t>(W),
                              ValueType::LOGICAL, mr);
    Value idx_out = Value::matrix(0, 1, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0)
        return std::make_tuple(std::move(out), std::move(idx_out));

    auto [L, K] = label_components(read_bw(BW), H, W, conn);
    if (K == 0) return std::make_tuple(std::move(out), std::move(idx_out));

    const size_t Nseed = std::min(cols.numel(), rows.numel());
    std::vector<uint8_t> keep(static_cast<size_t>(K) + 1, 0);
    for (size_t s = 0; s < Nseed; ++s) {
        const int c = static_cast<int>(cols.elemAsDouble(s)) - 1;
        const int r = static_cast<int>(rows.elemAsDouble(s)) - 1;
        if (r < 0 || r >= H || c < 0 || c >= W) continue;
        const int k = L[(size_t)r * (size_t)W + (size_t)c];
        if (k > 0) keep[(size_t)k] = 1;
    }

    uint8_t *od = out.logicalDataMut();
    std::vector<double> idx_v;
    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const int k = L[(size_t)r * (size_t)W + (size_t)c];
            const bool sel = (k > 0 && keep[(size_t)k]);
            od[(size_t)c * (size_t)H + (size_t)r] = sel ? 1u : 0u;
            if (sel) idx_v.push_back(double((size_t)c * (size_t)H + (size_t)r + 1));
        }
    idx_out = Value::matrix(idx_v.size(), 1, ValueType::DOUBLE, mr);
    if (!idx_v.empty()) std::copy(idx_v.begin(), idx_v.end(),
                                  idx_out.doubleDataMut());
    return std::make_tuple(std::move(out), std::move(idx_out));
}

Value bweuler(const Value &BW, int conn, std::pmr::memory_resource *mr)
{
    // Pratt bit-quad LUT (Octave-image bweuler.m).
    static constexpr int lut8[16] = {0, 1, 1, 0, 1, 0, -2, -1,
                                     1, -2, 0, -1, 0, -1, -1, 0};
    static constexpr int lut4[16] = {0, 1, 1, 0, 1, 0,  2, -1,
                                     1,  2, 0, -1, 0, -1, -1, 0};
    if (conn != 4 && conn != 8)
        throw Error("bweuler: connectivity must be 4 or 8",
                    0, 0, "bweuler", "", "numkit:bweuler:conn");
    const int *lut = (conn == 4) ? lut4 : lut8;

    const auto &d = BW.dims();
    if (d.is3D())
        throw Error("bweuler: BW must have 2 dimensions",
                    0, 0, "bweuler", "", "numkit:bweuler:dims");

    const int M = static_cast<int>(d.rows());
    const int N = static_cast<int>(d.cols());

    auto get = [&](int r, int c) -> int {
        if (r < 0 || c < 0 || r >= M || c >= N) return 0;
        return BW.elemAsDouble(static_cast<size_t>(c) *
                               static_cast<size_t>(M) +
                               static_cast<size_t>(r)) != 0.0 ? 1 : 0;
    };

    int sum = 0;
    // Padded image is (M+1)×(N+1) with zero top row/left col, so
    // every 2×2 window slides over (r, c) ∈ [0..M] × [0..N], reading
    // BW[r-1, c-1], BW[r, c-1], BW[r-1, c], BW[r, c] (clamped to 0
    // outside [0, M-1] × [0, N-1]).
    for (int r = 0; r <= M; ++r) {
        for (int c = 0; c <= N; ++c) {
            const int p00 = get(r - 1, c - 1);
            const int p10 = get(r,     c - 1);
            const int p01 = get(r - 1, c);
            const int p11 = get(r,     c);
            const int idx = p00 + 2 * p10 + 4 * p01 + 8 * p11;
            sum += lut[idx];
        }
    }
    return Value::scalar(static_cast<double>(sum) / 4.0, mr);
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
                    "numkit:bwlabel:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    auto [L, n] = bwlabel(args[0], conn, ctx.engine->resource());
    outs[0] = std::move(L);
    if (nargout > 1) outs[1] = std::move(n);
}

void bwconncomp_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwconncomp: requires (BW[, conn])", 0, 0, "bwconncomp", "",
                    "numkit:bwconncomp:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    outs[0] = bwconncomp(args[0], conn, ctx.engine->resource());
}

void labelmatrix_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("labelmatrix: requires (CC)", 0, 0, "labelmatrix", "",
                    "numkit:labelmatrix:nargin");
    outs[0] = labelmatrix(args[0], ctx.engine->resource());
}

void cc2bw_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cc2bw: requires (CC [, NV...])", 0, 0, "cc2bw", "",
                    "numkit:cc2bw:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    Value objs;  // empty → keep all
    std::size_t i = 1;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("cc2bw: expected NV-pair name string",
                        0, 0, "cc2bw", "", "numkit:cc2bw:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "objectstokeep") {
            objs = args[i + 1];
        } else {
            throw Error("cc2bw: unknown option '" + name + "'",
                        0, 0, "cc2bw", "",
                        "numkit:cc2bw:unknownNv");
        }
        i += 2;
    }
    outs[0] = cc2bw(args[0], objs, mr);
}

void bwarea_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwarea: requires BW", 0, 0, "bwarea", "",
                    "numkit:bwarea:nargin");
    outs[0] = bwarea(args[0], ctx.engine->resource());
}

void bwperim_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwperim: requires (BW[, conn])", 0, 0, "bwperim", "",
                    "numkit:bwperim:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    outs[0] = bwperim(args[0], conn, ctx.engine->resource());
}

void bwareaopen_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwareaopen: requires (BW, P[, conn])", 0, 0,
                    "bwareaopen", "", "numkit:bwareaopen:nargin");
    const int P = (int)args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? (int)args[2].toScalar() : 8;
    outs[0] = bwareaopen(args[0], P, conn, ctx.engine->resource());
}

void bwboundaries_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwboundaries: requires (BW[, conn])", 0, 0,
                    "bwboundaries", "", "numkit:bwboundaries:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    outs[0] = bwboundaries(args[0], conn, ctx.engine->resource());
}

void regionprops_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("regionprops: requires (BW_or_L[, props...])",
                    0, 0, "regionprops", "", "numkit:regionprops:nargin");
    std::vector<std::string> props;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("regionprops: property names must be strings",
                        0, 0, "regionprops", "", "numkit:regionprops:type");
        props.push_back(args[i].toString());
    }
    outs[0] = regionprops(args[0], props, ctx.engine->resource());
}

void bwdist_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwdist: requires (BW)",
                    0, 0, "bwdist", "", "numkit:bwdist:nargin");
    outs[0] = bwdist(args[0], ctx.engine->resource());
}

void roicolor_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("roicolor: requires (A, low, high) or (A, v)",
                    0, 0, "roicolor", "", "numkit:roicolor:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 3) {
        outs[0] = roicolor(args[0], args[1], args[2].toScalar(), /*is_range=*/true, mr);
    } else {
        outs[0] = roicolor(args[0], args[1], 0.0, /*is_range=*/false, mr);
    }
}

void fchcode_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fchcode: requires (bound)",
                    0, 0, "fchcode", "", "numkit:fchcode:nargin");
    outs[0] = fchcode(args[0], ctx.engine->resource());
}

void bwareafilt_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwareafilt: requires (BW, range|n [, keep] [, conn])",
                    0, 0, "bwareafilt", "", "numkit:bwareafilt:nargin");
    auto *mr = ctx.engine->resource();
    double lo = 0.0, hi = std::numeric_limits<double>::infinity();
    size_t n_keep = 0;
    bool keep_largest = true;
    int conn = 8;

    const Value &v = args[1];
    if (v.numel() >= 2) {
        // Range form.
        lo = v.elemAsDouble(0);
        hi = v.elemAsDouble(1);
    } else {
        n_keep = static_cast<size_t>(v.toScalar());
    }

    // Parse remaining args: optional keep_str + conn (any order valid?
    // Octave docs: keep before conn. Numeric → conn, string → keep.).
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isEmpty()) continue;
        if (a.isChar() || a.isString()) {
            const std::string s = a.toString();
            std::string lo_s;
            lo_s.reserve(s.size());
            for (char c : s) lo_s.push_back(static_cast<char>(std::tolower(c)));
            if      (lo_s == "largest")  keep_largest = true;
            else if (lo_s == "smallest") keep_largest = false;
            else throw Error("bwareafilt: keep must be 'largest' or 'smallest'",
                             0, 0, "bwareafilt", "", "numkit:bwareafilt:keep");
        } else if (a.numel() == 1) {
            conn = static_cast<int>(a.toScalar());
        }
    }

    outs[0] = bwareafilt(args[0], lo, hi, n_keep, keep_largest, conn, mr);
}

void bwselect_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bwselect: requires (BW, cols, rows[, conn])",
                    0, 0, "bwselect", "", "numkit:bwselect:nargin");
    int conn = 8;
    if (args.size() >= 4 && !args[3].isEmpty())
        conn = static_cast<int>(args[3].toScalar());
    auto [m, idx] = bwselect(args[0], args[1], args[2], conn, ctx.engine->resource());
    outs[0] = std::move(m);
    if (nargout > 1) outs[1] = std::move(idx);
}

void bweuler_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bweuler: requires (BW [, n])",
                    0, 0, "bweuler", "", "numkit:bweuler:nargin");
    int conn = 8;
    if (args.size() >= 2 && !args[1].isEmpty())
        conn = static_cast<int>(args[1].toScalar());
    outs[0] = bweuler(args[0], conn, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
