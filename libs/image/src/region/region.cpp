// libs/image/src/region/region.cpp

#include <numkit/image/region/region.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstdint>
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

} // namespace detail
} // namespace numkit::image
