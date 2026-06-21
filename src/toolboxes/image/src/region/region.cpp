// toolboxes/image/src/region/region.cpp

#include <numkit/image/region/region.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <tuple>

#include <algorithm>
#include <cctype>
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
    // convention). This nested loop visits every pixel, so it fully
    // relabels L — do NOT add a follow-up pass over L: by this point L
    // already holds remapped labels (1..K), and feeding those back through
    // find()/remap (which are keyed on the ORIGINAL provisional labels)
    // corrupts the result (returned count K stops matching max(L)).
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

Value bwboundaries(const Value &BW, int conn, Value *Lout, int *Nout, std::pmr::memory_resource *mr)
{
    if (conn != 4) conn = 8;
    const int H = (int)BW.dims().rows();
    const int W = (int)BW.dims().cols();
    auto fg = read_bw(BW);
    auto [L, K] = label_components(fg, H, W, conn);

    // Optional 2nd/3rd outputs: label matrix (objects 1..K, no holes) and
    // object count. The internal L is row-major; the Value is column-major.
    if (Nout) *Nout = K;
    if (Lout) {
        Value lab = Value::matrix(static_cast<size_t>(H),
                                  static_cast<size_t>(W), ValueType::DOUBLE, mr);
        double *ld = lab.doubleDataMut();
        for (int r = 0; r < H; ++r)
            for (int c = 0; c < W; ++c)
                ld[(size_t)c * (size_t)H + (size_t)r] =
                    static_cast<double>(L[(size_t)r * (size_t)W + (size_t)c]);
        *Lout = std::move(lab);
    }

    Value cellCol = Value::cell(static_cast<size_t>(K), 1, mr);
    if (K == 0) return cellCol;

    // 8-direction CW deltas starting at "right" (dir=0):
    //   E, SE, S, SW, W, NW, N, NE.
    static const int dr[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int dc[8] = { 1, 1, 0,-1,-1, -1,  0,  1 };
    auto inside = [&](int r, int c) {
        return r >= 0 && r < H && c >= 0 && c < W;
    };
    const auto &Lbl = L;   // plain reference: capturing a structured binding
                           // directly in a lambda is a C++20 extension.
    auto isFg = [&](int r, int c, int targetLabel) {
        if (!inside(r, c)) return false;
        return Lbl[(size_t)r * (size_t)W + (size_t)c] == targetLabel;
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

namespace {

// regionprops 'Perimeter' — MATLAB R2025b computePerimeterFromBoundary: trace
// the region's outer 8-connected boundary (Moore-neighbor) and apply the
// Vossepoel–Smeulders weighted estimator
//     perimeter = 0.980·Ne + 1.406·No − 0.091·Nc
// where Ne = axis steps, No = diagonal steps, Nc = orientation changes
// ("corners") around the closed boundary. The value is invariant to the trace
// start/direction, so we start at the region's top-left-most pixel (r0,c0) and
// go clockwise. Outer boundary only (holes not traced — MATLAB likewise warns
// Perimeter is meant for hole-free regions). Single-pixel regions return 0.
double regionPerimeter(const std::vector<int> &L, int H, int W,
                       int lab, int r0, int c0)
{
    static const int Dr8[8] = {-1, -1,  0,  1,  1,  1,  0, -1};
    static const int Dc8[8] = { 0,  1,  1,  1,  0, -1, -1, -1};
    auto fg = [&](int r, int c) {
        return r >= 0 && r < H && c >= 0 && c < W &&
               L[(size_t)r * (size_t)W + (size_t)c] == lab;
    };
    std::vector<int> br, bc;
    br.reserve(64);
    bc.reserve(64);
    br.push_back(r0);
    bc.push_back(c0);
    int back_dir = 6, cr = r0, cc = c0;   // back_dir = W (entered from background)
    const size_t cap = (size_t)H * (size_t)W * 4 + 8;
    while (br.size() < cap) {
        bool found = false;
        for (int k = 0; k < 8; ++k) {
            const int d = (back_dir + k + 1) % 8;          // clockwise from back_dir+1
            const int nr = cr + Dr8[d], nc = cc + Dc8[d];
            if (fg(nr, nc)) {
                br.push_back(nr);
                bc.push_back(nc);
                cr = nr;
                cc = nc;
                back_dir = (d + 4) % 8;                    // opposite of the move
                found = true;
                break;
            }
        }
        if (!found) break;                                  // isolated pixel
        if (cr == r0 && cc == c0 && br.size() >= 2) break;  // boundary closed
    }
    const size_t n = br.size();
    if (n < 3) return 0.0;                                  // ≤1 real step → 0
    const size_t steps = n - 1;
    auto stepType = [&](size_t i) {                         // 0 horiz, 1 vert, 2 diag
        const int dr = br[i + 1] - br[i], dc = bc[i + 1] - bc[i];
        return dr == 0 ? 0 : (dc == 0 ? 1 : 2);
    };
    long Ne = 0, No = 0, Nc = 0;
    for (size_t i = 0; i < steps; ++i) {
        const int t = stepType(i);
        if (t == 2) ++No; else ++Ne;
        if (t != stepType((i + 1) % steps)) ++Nc;           // circular corner count
    }
    return 0.980 * (double)Ne + 1.406 * (double)No - 0.091 * (double)Nc;
}

} // namespace

// ════════════════════════════════════════════════════════════════════
// regionprops — basic descriptors per labelled region
// ════════════════════════════════════════════════════════════════════

Value regionprops(const Value &BW_or_L, const std::vector<std::string> &propsIn, const Value &intensity, std::pmr::memory_resource *mr)
{
    const bool haveI = !intensity.isEmpty() && intensity.numel() > 0;
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
    // Reject unknown / unimplemented property names with a clear error instead
    // of silently dropping them (the silent drop surfaces later as a confusing
    // "non-existent field" on the result struct). Properties MATLAB ships but
    // numkit does not implement yet (Solidity, ConvexArea, EulerNumber,
    // FilledArea, Circularity, Extrema, ConvexHull, …) land here too. See
    // bugs/image/regionprops-perimeter.
    {
        static const char *kKnown[] = {
            "all", "basic",
            "area", "centroid", "boundingbox", "perimeter",
            "majoraxislength", "minoraxislength", "eccentricity", "orientation",
            "equivdiameter", "extent", "pixelidxlist", "pixellist",
            "meanintensity", "maxintensity", "minintensity",
            "weightedcentroid", "pixelvalues",
        };
        for (const auto &q : propsIn) {
            std::string lc = q;
            for (char &ch : lc)
                if (ch >= 'A' && ch <= 'Z') ch = char(ch + 32);
            bool ok = false;
            for (const char *k : kKnown)
                if (lc == k) { ok = true; break; }
            if (!ok)
                throw Error("regionprops: property '" + q +
                                "' is not supported in this revision",
                            0, 0, "regionprops", "",
                            "numkit:regionprops:badProperty");
        }
    }

    // MATLAB: an empty property list (or 'basic') returns the BASIC set
    // {Area, Centroid, BoundingBox}; 'all' adds every shape measurement.
    const bool basic   = propsIn.empty() || contains("basic");
    const bool wantAll = contains("all") || contains("All");
    const bool wArea = basic || wantAll || contains("Area");
    const bool wCent = basic || wantAll || contains("Centroid");
    const bool wBbox = basic || wantAll || contains("BoundingBox");
    // Moment / area / bbox based scalar shape descriptors (closed-form,
    // bit-exact vs MATLAB R2025b). The four ellipse fields share one set
    // of second central moments. NOT part of the basic default set.
    const bool wMajor   = wantAll || contains("MajorAxisLength");
    const bool wMinor   = wantAll || contains("MinorAxisLength");
    const bool wEcc     = wantAll || contains("Eccentricity");
    const bool wOrient  = wantAll || contains("Orientation");
    const bool wEquivD  = wantAll || contains("EquivDiameter");
    const bool wExtent  = wantAll || contains("Extent");
    const bool wPerim   = wantAll || contains("Perimeter");
    const bool wEllipse = wMajor || wMinor || wEcc || wOrient;
    // Per-pixel list fields (column-major linear indices / [x y] list).
    const bool wPixIdx  = wantAll || contains("PixelIdxList");
    const bool wPixList = wantAll || contains("PixelList");
    // Intensity measurements (only when a grayscale image is supplied).
    const bool wMeanI  = haveI && (wantAll || contains("MeanIntensity"));
    const bool wMaxI   = haveI && (wantAll || contains("MaxIntensity"));
    const bool wMinI   = haveI && (wantAll || contains("MinIntensity"));
    const bool wWCent  = haveI && (wantAll || contains("WeightedCentroid"));
    const bool wPixVal = haveI && (wantAll || contains("PixelValues"));
    const bool needIntensity = wMeanI || wMaxI || wMinI || wWCent || wPixVal;
    const bool needPixels = wPixIdx || wPixList || needIntensity;

    Value sa = Value::structArray(static_cast<size_t>(K), 1, mr);
    if (K == 0) return sa;

    // Accumulators per label.
    const bool needMoments = wEllipse;
    std::vector<long long> area(K + 1, 0);
    std::vector<double> sumX(K + 1, 0.0), sumY(K + 1, 0.0);
    // First pixel (row-major scan order = top-left-most) per label, the
    // boundary-trace start for Perimeter.
    std::vector<int> firstR(K + 1, -1), firstC(K + 1, -1);
    std::vector<double> sumXX(K + 1, 0.0), sumYY(K + 1, 0.0), sumXY(K + 1, 0.0);
    std::vector<int> minX(K + 1, INT_MAX), minY(K + 1, INT_MAX);
    std::vector<int> maxX(K + 1, INT_MIN), maxY(K + 1, INT_MIN);
    // Column-major 1-based linear indices per label (for PixelIdxList /
    // PixelList). Collected in row-major visit order then sorted ascending
    // to match MATLAB's column-major ordering.
    std::vector<std::vector<long long>> pixIdx(needPixels ? (K + 1) : 0);
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            const int lab = L[(size_t)r * (size_t)W + (size_t)c];
            if (lab <= 0 || lab > K) continue;
            ++area[(size_t)lab];
            if (firstR[(size_t)lab] < 0) { firstR[(size_t)lab] = r; firstC[(size_t)lab] = c; }
            sumX[(size_t)lab] += double(c);
            sumY[(size_t)lab] += double(r);
            if (needPixels)
                pixIdx[(size_t)lab].push_back(
                    static_cast<long long>(c) * H + r + 1);   // col-major 1-based
            if (needMoments) {
                sumXX[(size_t)lab] += double(c) * double(c);
                sumYY[(size_t)lab] += double(r) * double(r);
                sumXY[(size_t)lab] += double(c) * double(r);
            }
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

        // ── Moment / area / bbox based scalar shape descriptors ──
        constexpr double kPi = 3.14159265358979323846;
        const double N = double(area[(size_t)lab]);
        if (wEquivD)
            el.emplace("EquivDiameter",
                       Value::scalar((N > 0.0) ? std::sqrt(4.0 * N / kPi) : 0.0, mr));
        if (wExtent) {
            const double bw = double(maxX[(size_t)lab] - minX[(size_t)lab] + 1);
            const double bh = double(maxY[(size_t)lab] - minY[(size_t)lab] + 1);
            const double bba = bw * bh;
            el.emplace("Extent",
                       Value::scalar((bba > 0.0) ? N / bba : 0.0, mr));
        }
        if (wPerim) {
            const double per = (area[(size_t)lab] > 0 && firstR[(size_t)lab] >= 0)
                ? regionPerimeter(L, H, W, lab,
                                  firstR[(size_t)lab], firstC[(size_t)lab])
                : 0.0;
            el.emplace("Perimeter", Value::scalar(per, mr));
        }
        if (wEllipse) {
            // Normalized second central moments with the +1/12 per-pixel
            // variance correction (uniform unit-pixel), matching MATLAB
            // regionprops. y is flipped for the orientation because image
            // rows increase downward.
            double major = 0.0, minor = 0.0, ecc = 0.0, orient = 0.0;
            if (N > 0.0) {
                const double xbar = sumX[(size_t)lab] / N;
                const double ybar = sumY[(size_t)lab] / N;
                const double uxx = sumXX[(size_t)lab] / N - xbar * xbar + 1.0 / 12.0;
                const double uyy = sumYY[(size_t)lab] / N - ybar * ybar + 1.0 / 12.0;
                const double uxy = sumXY[(size_t)lab] / N - xbar * ybar;
                const double common =
                    std::sqrt((uxx - uyy) * (uxx - uyy) + 4.0 * uxy * uxy);
                major = 2.0 * std::sqrt(2.0) * std::sqrt(uxx + uyy + common);
                minor = 2.0 * std::sqrt(2.0) * std::sqrt(uxx + uyy - common);
                if (major > 0.0) {
                    const double a = major / 2.0, b = minor / 2.0;
                    const double d = a * a - b * b;
                    ecc = 2.0 * std::sqrt(d > 0.0 ? d : 0.0) / major;
                }
                // Orientation in degrees, range [-90, 90].
                if (uxy == 0.0) {
                    orient = (uyy > uxx) ? 90.0 : 0.0;  // axis-aligned
                } else {
                    const double uxyM = -uxy;  // flip y (image rows go down)
                    double num, den;
                    if (uyy > uxx) {
                        num = uyy - uxx +
                              std::sqrt((uyy - uxx) * (uyy - uxx) + 4.0 * uxyM * uxyM);
                        den = 2.0 * uxyM;
                    } else {
                        num = 2.0 * uxyM;
                        den = uxx - uyy +
                              std::sqrt((uxx - uyy) * (uxx - uyy) + 4.0 * uxyM * uxyM);
                    }
                    orient = (180.0 / kPi) * std::atan(num / den);
                }
            }
            if (wMajor)  el.emplace("MajorAxisLength", Value::scalar(major, mr));
            if (wMinor)  el.emplace("MinorAxisLength", Value::scalar(minor, mr));
            if (wEcc)    el.emplace("Eccentricity",    Value::scalar(ecc, mr));
            if (wOrient) el.emplace("Orientation",     Value::scalar(orient, mr));
        }

        // ── Per-pixel list fields ──
        if (needPixels) {
            auto &idxList = pixIdx[(size_t)lab];
            std::sort(idxList.begin(), idxList.end());   // MATLAB col-major order
            const size_t P = idxList.size();
            if (wPixIdx) {
                Value pil = Value::matrix(P, 1, ValueType::DOUBLE, mr);
                double *pd = pil.doubleDataMut();
                for (size_t i = 0; i < P; ++i)
                    pd[i] = static_cast<double>(idxList[i]);
                el.emplace("PixelIdxList", std::move(pil));
            }
            if (wPixList) {
                // [x y] = [col row], 1-based, derived from the sorted index.
                Value plv = Value::matrix(P, 2, ValueType::DOUBLE, mr);
                double *pl = plv.doubleDataMut();
                for (size_t i = 0; i < P; ++i) {
                    const long long z = idxList[i] - 1;     // 0-based col-major
                    const long long row0 = z % H;
                    const long long col0 = z / H;
                    pl[i]     = static_cast<double>(col0 + 1);   // x = col
                    pl[P + i] = static_cast<double>(row0 + 1);   // y = row
                }
                el.emplace("PixelList", std::move(plv));
            }
            // ── Intensity measurements (grayscale image supplied) ──
            if (needIntensity) {
                double sumI = 0.0, sumIx = 0.0, sumIy = 0.0;
                double maxI = -std::numeric_limits<double>::infinity();
                double minI =  std::numeric_limits<double>::infinity();
                Value pvv;
                double *pv = nullptr;
                if (wPixVal) {
                    pvv = Value::matrix(P, 1, ValueType::DOUBLE, mr);
                    pv = pvv.doubleDataMut();
                }
                for (size_t i = 0; i < P; ++i) {
                    const long long z = idxList[i] - 1;     // col-major 0-based
                    const double iv = intensity.elemAsDouble(static_cast<size_t>(z));
                    sumI += iv;
                    if (iv > maxI) maxI = iv;
                    if (iv < minI) minI = iv;
                    if (wWCent) {
                        const long long row0 = z % H, col0 = z / H;
                        sumIx += iv * static_cast<double>(col0 + 1);
                        sumIy += iv * static_cast<double>(row0 + 1);
                    }
                    if (pv) pv[i] = iv;
                }
                if (wMeanI) el.emplace("MeanIntensity",
                                       Value::scalar(P ? sumI / double(P) : 0.0, mr));
                if (wMaxI)  el.emplace("MaxIntensity", Value::scalar(maxI, mr));
                if (wMinI)  el.emplace("MinIntensity", Value::scalar(minI, mr));
                if (wWCent) {
                    Value wc = Value::matrix(1, 2, ValueType::DOUBLE, mr);
                    wc.doubleDataMut()[0] = (sumI != 0.0) ? sumIx / sumI : 0.0;
                    wc.doubleDataMut()[1] = (sumI != 0.0) ? sumIy / sumI : 0.0;
                    el.emplace("WeightedCentroid", std::move(wc));
                }
                if (wPixVal) el.emplace("PixelValues", std::move(pvv));
            }
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

} // namespace numkit::image
