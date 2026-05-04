// libs/image/src/segment/segment.cpp
//
// Lightweight segmentation utilities — no DL, no fancy numerics.
// Each entry is O(N) on the input. Composes with the cycle-22
// bwlabel / regionprops infrastructure.

#include <numkit/image/segment/segment.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::image {

namespace {

bool nonZero(double v) { return v != 0.0; }

void requireSameShape(const Value &A, const Value &B, const char *fn) {
    if (A.numel() != B.numel() ||
        A.dims().rows() != B.dims().rows() ||
        A.dims().cols() != B.dims().cols())
        throw Error(std::string(fn) + ": inputs must have the same shape",
                    0, 0, fn, "", "m:image:segment:shape");
}

} // anonymous

Value dice(std::pmr::memory_resource *mr, const Value &A, const Value &B)
{
    requireSameShape(A, B, "dice");
    const size_t N = A.numel();
    long long inter = 0, sumA = 0, sumB = 0;
    for (size_t i = 0; i < N; ++i) {
        const bool a = nonZero(A.elemAsDouble(i));
        const bool b = nonZero(B.elemAsDouble(i));
        if (a) ++sumA;
        if (b) ++sumB;
        if (a && b) ++inter;
    }
    const long long denom = sumA + sumB;
    const double d = (denom == 0) ? 1.0
                                  : 2.0 * double(inter) / double(denom);
    return Value::scalar(d, mr);
}

Value jaccard(std::pmr::memory_resource *mr, const Value &A, const Value &B)
{
    requireSameShape(A, B, "jaccard");
    const size_t N = A.numel();
    long long inter = 0, uni = 0;
    for (size_t i = 0; i < N; ++i) {
        const bool a = nonZero(A.elemAsDouble(i));
        const bool b = nonZero(B.elemAsDouble(i));
        if (a && b) ++inter;
        if (a || b) ++uni;
    }
    const double j = (uni == 0) ? 1.0
                                : double(inter) / double(uni);
    return Value::scalar(j, mr);
}

Value boundarymask(std::pmr::memory_resource *mr,
                   const Value &L, int conn)
{
    if (conn != 4) conn = 8;
    const int H = static_cast<int>(L.dims().rows());
    const int W = static_cast<int>(L.dims().cols());
    Value out = Value::matrix(static_cast<size_t>(H),
                              static_cast<size_t>(W),
                              ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;

    // Read L as int (round); column-major access via elemAsDouble.
    auto labAt = [&](int r, int c) -> long long {
        return static_cast<long long>(std::round(
            L.elemAsDouble(static_cast<size_t>(c) * static_cast<size_t>(H) +
                            static_cast<size_t>(r))));
    };

    std::uint8_t *od = out.logicalDataMut();
    static const int dr8[8] = { -1,-1,-1, 0, 0, 1, 1, 1 };
    static const int dc8[8] = { -1, 0, 1,-1, 1,-1, 0, 1 };
    static const int dr4[4] = { -1, 1, 0, 0 };
    static const int dc4[4] = {  0, 0,-1, 1 };
    const int *dr = (conn == 4) ? dr4 : dr8;
    const int *dc = (conn == 4) ? dc4 : dc8;
    const int K   = (conn == 4) ? 4    : 8;

    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const long long me = labAt(r, c);
            // Background pixel (label 0) is never a boundary in the
            // MATLAB sense; for a pure binary mask we treat the
            // foreground perimeter.
            bool isBoundary = false;
            for (int k = 0; k < K; ++k) {
                const int nr = r + dr[k];
                const int nc = c + dc[k];
                if (nr < 0 || nr >= H || nc < 0 || nc >= W) {
                    if (me != 0) { isBoundary = true; break; }
                    continue;
                }
                const long long ne = labAt(nr, nc);
                if (ne != me) { isBoundary = true; break; }
            }
            // Background-only neighbourhood: not a boundary.
            if (me == 0) isBoundary = false;
            od[static_cast<size_t>(c) * static_cast<size_t>(H) +
               static_cast<size_t>(r)] = isBoundary ? 1u : 0u;
        }
    return out;
}

Value label2idx(std::pmr::memory_resource *mr, const Value &L)
{
    const size_t N = L.numel();
    long long maxLab = 0;
    for (size_t i = 0; i < N; ++i) {
        const long long v = static_cast<long long>(
            std::round(L.elemAsDouble(i)));
        if (v > maxLab) maxLab = v;
    }
    Value cell = Value::cell(static_cast<size_t>(maxLab), 1, mr);
    if (maxLab == 0) return cell;

    // First pass: count per label.
    std::vector<size_t> counts(static_cast<size_t>(maxLab) + 1, 0);
    for (size_t i = 0; i < N; ++i) {
        const long long v = static_cast<long long>(
            std::round(L.elemAsDouble(i)));
        if (v > 0) ++counts[static_cast<size_t>(v)];
    }
    // Allocate per-label column vectors (1-based indices).
    std::vector<Value> bufs;
    bufs.reserve(static_cast<size_t>(maxLab));
    std::vector<double *> wptr(static_cast<size_t>(maxLab) + 1, nullptr);
    std::vector<size_t> cur(static_cast<size_t>(maxLab) + 1, 0);
    for (long long k = 1; k <= maxLab; ++k) {
        Value v = Value::matrix(counts[static_cast<size_t>(k)], 1,
                                ValueType::DOUBLE, mr);
        wptr[static_cast<size_t>(k)] = (counts[static_cast<size_t>(k)] > 0)
                                       ? v.doubleDataMut() : nullptr;
        bufs.push_back(std::move(v));
    }
    // Second pass: fill.
    for (size_t i = 0; i < N; ++i) {
        const long long v = static_cast<long long>(
            std::round(L.elemAsDouble(i)));
        if (v > 0) {
            wptr[static_cast<size_t>(v)][cur[static_cast<size_t>(v)]] =
                static_cast<double>(i + 1); // MATLAB 1-based
            ++cur[static_cast<size_t>(v)];
        }
    }
    for (long long k = 1; k <= maxLab; ++k)
        cell.cellAt(static_cast<size_t>(k - 1)) =
            std::move(bufs[static_cast<size_t>(k - 1)]);
    return cell;
}

namespace detail {

void dice_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("dice: requires (BW1, BW2)",
                    0, 0, "dice", "", "m:dice:nargin");
    o[0] = dice(c.engine->resource(), a[0], a[1]);
}

void jaccard_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("jaccard: requires (BW1, BW2)",
                    0, 0, "jaccard", "", "m:jaccard:nargin");
    o[0] = jaccard(c.engine->resource(), a[0], a[1]);
}

void boundarymask_reg(Span<const Value> a, size_t, Span<Value> o,
                      CallContext &c)
{
    if (a.empty())
        throw Error("boundarymask: requires (L_or_BW [, conn])",
                    0, 0, "boundarymask", "", "m:boundarymask:nargin");
    const int conn = (a.size() >= 2 && !a[1].isEmpty())
                     ? static_cast<int>(a[1].toScalar()) : 8;
    o[0] = boundarymask(c.engine->resource(), a[0], conn);
}

void label2idx_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("label2idx: requires (L)",
                    0, 0, "label2idx", "", "m:label2idx:nargin");
    o[0] = label2idx(c.engine->resource(), a[0]);
}

} // namespace detail

} // namespace numkit::image
