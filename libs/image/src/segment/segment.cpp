// libs/image/src/segment/segment.cpp
//
// Lightweight segmentation utilities — no DL, no fancy numerics.
// Each entry is O(N) on the input. Composes with the cycle-22
// bwlabel / regionprops infrastructure.

#include <numkit/image/segment/segment.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
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

Value dice(const Value &A, const Value &B, std::pmr::memory_resource *mr)
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

Value jaccard(const Value &A, const Value &B, std::pmr::memory_resource *mr)
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

Value boundarymask(const Value &L, int conn, std::pmr::memory_resource *mr)
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

Value label2idx(const Value &L, std::pmr::memory_resource *mr)
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

Value grayconnected(const Value &I, int row, int col, double tol, std::pmr::memory_resource *mr)
{
    const int H = static_cast<int>(I.dims().rows());
    const int W = static_cast<int>(I.dims().cols());
    Value out = Value::matrix(static_cast<size_t>(H),
                              static_cast<size_t>(W),
                              ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;

    // 1-based MATLAB → 0-based.
    const int r0 = row - 1;
    const int c0 = col - 1;
    if (r0 < 0 || r0 >= H || c0 < 0 || c0 >= W)
        throw Error("grayconnected: seed out of bounds",
                    0, 0, "grayconnected", "", "m:grayconnected:seed");

    // Auto-pick tolerance if caller passed a negative sentinel.
    if (tol < 0.0) {
        switch (I.type()) {
            case ValueType::UINT8:  tol = 32.0;            break;
            case ValueType::INT8:   tol = 32.0;            break;
            case ValueType::UINT16: tol = 32.0 * 256.0;    break;
            case ValueType::INT16:  tol = 32.0 * 256.0;    break;
            default:                tol = 0.125;            break; // [0, 1] floats
        }
    }

    auto idxAt = [&](int r, int c) {
        return static_cast<size_t>(c) * static_cast<size_t>(H) +
               static_cast<size_t>(r);
    };
    const double seedVal = I.elemAsDouble(idxAt(r0, c0));
    std::uint8_t *od = out.logicalDataMut();
    std::vector<std::uint8_t> visited(static_cast<size_t>(H) *
                                       static_cast<size_t>(W), 0);

    // BFS with an explicit deque (vector + read head).
    std::vector<std::pair<int, int>> q;
    q.reserve(static_cast<size_t>(H) * static_cast<size_t>(W) / 4 + 4);
    q.emplace_back(r0, c0);
    visited[static_cast<size_t>(r0) * static_cast<size_t>(W) +
            static_cast<size_t>(c0)] = 1;
    od[idxAt(r0, c0)] = 1;
    static const int dr8[8] = { -1,-1,-1, 0, 0, 1, 1, 1 };
    static const int dc8[8] = { -1, 0, 1,-1, 1,-1, 0, 1 };

    size_t head = 0;
    while (head < q.size()) {
        const auto [r, c] = q[head++];
        for (int k = 0; k < 8; ++k) {
            const int nr = r + dr8[k];
            const int nc = c + dc8[k];
            if (nr < 0 || nr >= H || nc < 0 || nc >= W) continue;
            const size_t fi = static_cast<size_t>(nr) *
                                static_cast<size_t>(W) +
                              static_cast<size_t>(nc);
            if (visited[fi]) continue;
            const double v = I.elemAsDouble(idxAt(nr, nc));
            if (std::abs(v - seedVal) > tol) continue;
            visited[fi] = 1;
            od[idxAt(nr, nc)] = 1;
            q.emplace_back(nr, nc);
        }
    }
    return out;
}

// ── graydiffweight (FMM intensity-difference weights) ──────────────
//
// MATLAB R2025b graydiffweight.m → call sequence:
//   d = |I − refGrayVal|
//   if cutoff < Inf: isSuppressed = (d > cutoff)
//   d_scaled = imlinscale(d, [1e-3, 1])    — linear (min, max)→(1e-3, 1)
//   if cutoff: d_scaled(isSuppressed) = 1
//   W = 1 ./ (d_scaled .^ (1 / rolloffFactor))
// Output class is single if input was single, otherwise double.
// (The 4 input signatures collapse to a single scalar `refGrayVal`
// here; the adapter computes the mean over MASK / (C, R) / (C, R, P)
// before dispatching to this typed entry-point.)
Value graydiffweight(const Value &I, double ref_gray_val,
                     double rolloff_factor, double cutoff,
                     std::pmr::memory_resource *mr)
{
    if (!(rolloff_factor > 0.0))
        throw Error("graydiffweight: RolloffFactor must be positive",
                    0, 0, "graydiffweight", "", "m:graydiffweight:rolloff");
    if (!(cutoff >= 0.0))
        throw Error("graydiffweight: GrayDifferenceCutoff must be "
                    "non-negative",
                    0, 0, "graydiffweight", "", "m:graydiffweight:cutoff");

    const ValueType outT = (I.type() == ValueType::SINGLE)
                          ? ValueType::SINGLE : ValueType::DOUBLE;
    const auto &d = I.dims();
    const std::size_t N = I.numel();
    Value W = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), outT, mr)
        : Value::matrix(d.rows(), d.cols(), outT, mr);
    if (N == 0) return W;

    // Pass 1 — compute |I − ref| in DOUBLE temporary; track min/max.
    std::pmr::vector<double> diff(N, mr);
    double dmin = std::numeric_limits<double>::infinity();
    double dmax = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < N; ++i) {
        const double v = std::fabs(I.elemAsDouble(i) - ref_gray_val);
        diff[i] = v;
        if (v < dmin) dmin = v;
        if (v > dmax) dmax = v;
    }

    // Linear scale to [1e-3, 1].
    const double lo = 1e-3, hi = 1.0;
    const double range = dmax - dmin;
    const double slope = (range > 0.0) ? (hi - lo) / range : 0.0;

    const double inv_rolloff = 1.0 / rolloff_factor;
    const bool finite_cutoff = std::isfinite(cutoff);

    auto store = [&](std::size_t i, double v) {
        if (outT == ValueType::DOUBLE) W.doubleDataMut()[i] = v;
        else                            W.singleDataMut()[i] = static_cast<float>(v);
    };

    for (std::size_t i = 0; i < N; ++i) {
        double s;
        if (range > 0.0) s = lo + slope * (diff[i] - dmin);
        else             s = lo;     // constant-image edge case
        if (finite_cutoff && diff[i] > cutoff) s = hi;
        const double w = 1.0 / std::pow(s, inv_rolloff);
        store(i, w);
    }
    return W;
}

Value imoverlay(const Value &I, const Value &BW, const Value &color, std::pmr::memory_resource *mr)
{
    if (color.numel() != 3)
        throw Error("imoverlay: color must be a 1×3 RGB triple",
                    0, 0, "imoverlay", "", "m:imoverlay:color");
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    if (BW.dims().rows() != H || BW.dims().cols() != W)
        throw Error("imoverlay: BW must match the H × W of I",
                    0, 0, "imoverlay", "", "m:imoverlay:shape");

    // Detect input layout: H × W (grayscale) or H × W × 3 (RGB).
    bool isRGB;
    if (I.numel() == H * W) isRGB = false;
    else if (I.numel() == H * W * 3) isRGB = true;
    else
        throw Error("imoverlay: I must be H × W or H × W × 3",
                    0, 0, "imoverlay", "", "m:imoverlay:shape");

    // Read color. Auto-detect float (0..1) vs byte (0..255) by max
    // value: if all three channels ≤ 1.0 we assume 0..1, else 0..255.
    double cIn[3] = { color.elemAsDouble(0),
                       color.elemAsDouble(1),
                       color.elemAsDouble(2) };
    const bool floatColour = (cIn[0] <= 1.0 && cIn[1] <= 1.0 &&
                               cIn[2] <= 1.0);
    int cByte[3];
    for (int k = 0; k < 3; ++k) {
        double v = floatColour ? cIn[k] * 255.0 : cIn[k];
        if (v < 0.0) v = 0.0;
        if (v > 255.0) v = 255.0;
        cByte[k] = static_cast<int>(std::lround(v));
    }

    // Helper: read I element as 0..255 byte.
    const ValueType srcT = I.type();
    auto pixelByte = [&](size_t y, size_t x, int chan) {
        size_t idx;
        if (isRGB)
            idx = static_cast<size_t>(chan) * H * W + x * H + y;
        else
            idx = x * H + y;
        const double v = I.elemAsDouble(idx);
        double w = v;
        if (srcT != ValueType::UINT8 && srcT != ValueType::UINT16 &&
            srcT != ValueType::INT8  && srcT != ValueType::INT16) {
            // Floating point: assume [0, 1].
            w = v * 255.0;
        } else if (srcT == ValueType::UINT16) {
            w = v / 257.0;
        } else if (srcT == ValueType::INT16) {
            w = (v + 32768.0) / 257.0;
        }
        if (w < 0.0) w = 0.0;
        if (w > 255.0) w = 255.0;
        return static_cast<std::uint8_t>(std::lround(w));
    };

    Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
    std::uint8_t *od = out.uint8DataMut();
    const size_t plane = H * W;
    for (size_t y = 0; y < H; ++y)
        for (size_t x = 0; x < W; ++x) {
            const bool flag = (BW.elemAsDouble(x * H + y) != 0.0);
            for (int c = 0; c < 3; ++c) {
                const size_t outIdx = static_cast<size_t>(c) * plane +
                                       x * H + y;
                if (flag)
                    od[outIdx] = static_cast<std::uint8_t>(cByte[c]);
                else
                    od[outIdx] = pixelByte(y, x, isRGB ? c : 0);
            }
        }
    return out;
}

namespace detail {

void imoverlay_reg(Span<const Value> a, size_t, Span<Value> o,
                   CallContext &c)
{
    if (a.size() < 3)
        throw Error("imoverlay: requires (I, BW, color)",
                    0, 0, "imoverlay", "", "m:imoverlay:nargin");
    o[0] = imoverlay(a[0], a[1], a[2], c.engine->resource());
}

void grayconnected_reg(Span<const Value> a, size_t, Span<Value> o,
                       CallContext &c)
{
    if (a.size() < 3)
        throw Error("grayconnected: requires (I, row, col [, tol])",
                    0, 0, "grayconnected", "", "m:grayconnected:nargin");
    const int row = static_cast<int>(a[1].toScalar());
    const int col = static_cast<int>(a[2].toScalar());
    double tol = -1.0;
    if (a.size() >= 4 && !a[3].isEmpty()) tol = a[3].toScalar();
    o[0] = grayconnected(a[0], row, col, tol, c.engine->resource());
}

void dice_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("dice: requires (BW1, BW2)",
                    0, 0, "dice", "", "m:dice:nargin");
    o[0] = dice(a[0], a[1], c.engine->resource());
}

void jaccard_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("jaccard: requires (BW1, BW2)",
                    0, 0, "jaccard", "", "m:jaccard:nargin");
    o[0] = jaccard(a[0], a[1], c.engine->resource());
}

void boundarymask_reg(Span<const Value> a, size_t, Span<Value> o,
                      CallContext &c)
{
    if (a.empty())
        throw Error("boundarymask: requires (L_or_BW [, conn])",
                    0, 0, "boundarymask", "", "m:boundarymask:nargin");
    const int conn = (a.size() >= 2 && !a[1].isEmpty())
                     ? static_cast<int>(a[1].toScalar()) : 8;
    o[0] = boundarymask(a[0], conn, c.engine->resource());
}

// graydiffweight adapter — handles all 4 input signatures plus the
// 'RolloffFactor' / 'GrayDifferenceCutoff' name-value pairs by
// computing the scalar reference value upfront and dispatching to
// the typed entry-point.
void graydiffweight_reg(Span<const Value> a, size_t, Span<Value> o,
                        CallContext &c)
{
    if (a.size() < 2)
        throw Error("graydiffweight: requires (I, refGrayVal | MASK | "
                    "C, R [, P]) [, NV...]",
                    0, 0, "graydiffweight", "", "m:graydiffweight:nargin");
    auto *mr = c.engine->resource();
    const Value &I = a[0];
    const auto &d = I.dims();
    const std::size_t N = I.numel();
    const std::size_t H = d.rows();
    const std::size_t W_ = d.cols();
    const std::size_t P = d.is3D() ? d.pages() : 1;

    // Identify which signature is in play, then determine where the
    // name-value pairs start.
    double ref_gray_val = 0.0;
    std::size_t nv_start = 2;

    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    const bool a1_str = is_string(a[1]);

    if (!a1_str && a[1].isLogical()) {
        // (I, MASK) — mean of I over MASK = true.
        if (a[1].numel() != N)
            throw Error("graydiffweight: MASK must match I in shape",
                        0, 0, "graydiffweight", "", "m:graydiffweight:mask");
        long double sum = 0.0L; std::size_t cnt = 0;
        const std::uint8_t *m = a[1].logicalData();
        for (std::size_t i = 0; i < N; ++i)
            if (m[i]) { sum += I.elemAsDouble(i); ++cnt; }
        if (cnt == 0)
            throw Error("graydiffweight: MASK must contain at least one true",
                        0, 0, "graydiffweight", "", "m:graydiffweight:emptyMask");
        ref_gray_val = static_cast<double>(sum / static_cast<long double>(cnt));
        nv_start = 2;
    }
    else if (!a1_str && a[1].numel() == 1 &&
             (a.size() < 3 || a[2].isChar() || a[2].isString())) {
        // (I, refGrayVal) — scalar reference.
        ref_gray_val = a[1].toScalar();
        nv_start = 2;
    }
    else if (!a1_str && a.size() >= 3 && !is_string(a[2])) {
        // (I, C, R [, P]) — mean over indexed pixels.
        const Value &C = a[1], &R = a[2];
        const bool has_P = (a.size() >= 4 && !is_string(a[3]));
        const Value *Pi = has_P ? &a[3] : nullptr;
        if (C.numel() != R.numel() || (Pi && Pi->numel() != C.numel()))
            throw Error("graydiffweight: C, R [, P] must have equal length",
                        0, 0, "graydiffweight", "", "m:graydiffweight:crp");
        long double sum = 0.0L; std::size_t cnt = 0;
        for (std::size_t i = 0; i < C.numel(); ++i) {
            const std::size_t c1 = static_cast<std::size_t>(C.elemAsDouble(i));
            const std::size_t r1 = static_cast<std::size_t>(R.elemAsDouble(i));
            const std::size_t p1 = Pi ? static_cast<std::size_t>(Pi->elemAsDouble(i))
                                       : 1;
            if (c1 < 1 || c1 > W_ || r1 < 1 || r1 > H || p1 < 1 || p1 > P)
                throw Error("graydiffweight: (C, R [, P]) index out of range",
                            0, 0, "graydiffweight", "",
                            "m:graydiffweight:idx");
            // Column-major linear: r-1 + H*(c-1) + H*W*(p-1).
            const std::size_t lin = (r1 - 1) + H * (c1 - 1)
                                  + H * W_ * (p1 - 1);
            sum += I.elemAsDouble(lin);
            ++cnt;
        }
        ref_gray_val = static_cast<double>(sum / static_cast<long double>(cnt));
        nv_start = has_P ? 4 : 3;
    }
    else {
        throw Error("graydiffweight: 2nd argument must be a numeric "
                    "scalar, a logical MASK, or numeric C [, R [, P]]",
                    0, 0, "graydiffweight", "", "m:graydiffweight:arg2");
    }

    // Name-value pairs.
    double rolloff = 0.5;
    double cutoff  = std::numeric_limits<double>::infinity();
    std::size_t i = nv_start;
    while (i + 1 < a.size()) {
        if (!is_string(a[i]))
            throw Error("graydiffweight: expected NV-pair name string",
                        0, 0, "graydiffweight", "",
                        "m:graydiffweight:badNvArg");
        std::string name = a[i].toString();
        std::string nlo = name;
        for (auto &ch : nlo)
            ch = static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        // Allow MATLAB-style abbreviation.
        if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "roll") == 0)
            rolloff = a[i + 1].toScalar();
        else if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "gray") == 0)
            cutoff = a[i + 1].toScalar();
        else
            throw Error("graydiffweight: unknown option '" + name + "'",
                        0, 0, "graydiffweight", "",
                        "m:graydiffweight:unknownNv");
        i += 2;
    }
    if (i < a.size())
        throw Error("graydiffweight: trailing unpaired NV argument",
                    0, 0, "graydiffweight", "",
                    "m:graydiffweight:unpaired");

    o[0] = graydiffweight(I, ref_gray_val, rolloff, cutoff, mr);
}

void label2idx_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("label2idx: requires (L)",
                    0, 0, "label2idx", "", "m:label2idx:nargin");
    o[0] = label2idx(a[0], c.engine->resource());
}

} // namespace detail

} // namespace numkit::image
