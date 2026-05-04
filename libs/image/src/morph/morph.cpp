// libs/image/src/morph/morph.cpp

#include <numkit/image/morph/morph.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::image {

namespace {

Value pack_logical(std::pmr::memory_resource *mr,
                   const std::vector<uint8_t> &mask, int rows, int cols)
{
    Value out = Value::matrix(rows, cols, ValueType::LOGICAL, mr);
    if (mask.empty()) return out;
    uint8_t *od = out.logicalDataMut();
    // Input is row-major mask; storage is column-major.
    for (int c = 0; c < cols; ++c)
        for (int r = 0; r < rows; ++r)
            od[(size_t)c * (size_t)rows + (size_t)r]
                = mask[(size_t)r * (size_t)cols + (size_t)c] ? 1 : 0;
    return out;
}

Value strel_square(std::pmr::memory_resource *mr, int N) {
    if (N < 1) N = 1;
    std::vector<uint8_t> m((size_t)N * (size_t)N, 1);
    return pack_logical(mr, m, N, N);
}

Value strel_rect(std::pmr::memory_resource *mr, int rows, int cols) {
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    std::vector<uint8_t> m((size_t)rows * (size_t)cols, 1);
    return pack_logical(mr, m, rows, cols);
}

Value strel_diamond(std::pmr::memory_resource *mr, int r) {
    if (r < 1) r = 1;
    const int N = 2 * r + 1;
    std::vector<uint8_t> m((size_t)N * (size_t)N, 0);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            if (std::abs(i - r) + std::abs(j - r) <= r)
                m[(size_t)i * (size_t)N + (size_t)j] = 1;
    return pack_logical(mr, m, N, N);
}

Value strel_disk(std::pmr::memory_resource *mr, double r) {
    if (r < 1.0) r = 1.0;
    const int R = (int)std::ceil(r);
    const int N = 2 * R + 1;
    std::vector<uint8_t> m((size_t)N * (size_t)N, 0);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j) {
            const double dy = i - R, dx = j - R;
            if (std::sqrt(dy * dy + dx * dx) <= r)
                m[(size_t)i * (size_t)N + (size_t)j] = 1;
        }
    return pack_logical(mr, m, N, N);
}

Value strel_line(std::pmr::memory_resource *mr, double len, double theta_deg) {
    if (len < 1.0) len = 1.0;
    const double th = theta_deg * M_PI / 180.0;
    const double cx = (len - 1) * 0.5 * std::cos(th);
    const double cy = (len - 1) * 0.5 * std::sin(th);
    const int W = std::max(1, (int)std::ceil(2.0 * std::fabs(cx) + 1.0));
    const int H = std::max(1, (int)std::ceil(2.0 * std::fabs(cy) + 1.0));
    std::vector<uint8_t> m((size_t)H * (size_t)W, 0);
    const double r0 = (H - 1) * 0.5;
    const double c0 = (W - 1) * 0.5;
    const int Nstep = std::max(1, (int)std::ceil(len));
    for (int k = 0; k < Nstep; ++k) {
        const double t = (Nstep > 1) ? (double(k) / double(Nstep - 1) - 0.5) * (len - 1) : 0.0;
        const int rr = (int)std::round(r0 + t * std::sin(th));
        const int cc = (int)std::round(c0 + t * std::cos(th));
        if (rr >= 0 && rr < H && cc >= 0 && cc < W)
            m[(size_t)rr * (size_t)W + (size_t)cc] = 1;
    }
    return pack_logical(mr, m, H, W);
}

} // anonymous

Value strel(std::pmr::memory_resource *mr,
            const std::string &shape,
            const std::vector<double> &params,
            const Value &arbitrary_nhood)
{
    if (shape == "square") {
        const int N = params.empty() ? 3 : (int)params[0];
        return strel_square(mr, N);
    }
    if (shape == "rectangle") {
        const int r = params.size() >= 1 ? (int)params[0] : 3;
        const int c = params.size() >= 2 ? (int)params[1] : r;
        return strel_rect(mr, r, c);
    }
    if (shape == "diamond") {
        const int r = params.empty() ? 1 : (int)params[0];
        return strel_diamond(mr, r);
    }
    if (shape == "disk") {
        const double r = params.empty() ? 5.0 : params[0];
        return strel_disk(mr, r);
    }
    if (shape == "line") {
        const double len = params.size() >= 1 ? params[0] : 3.0;
        const double th  = params.size() >= 2 ? params[1] : 0.0;
        return strel_line(mr, len, th);
    }
    if (shape == "arbitrary" || shape.empty()) {
        if (arbitrary_nhood.numel() == 0)
            throw Error("strel('arbitrary', NHOOD): NHOOD missing",
                        0, 0, "strel", "", "m:strel:nargin");
        const auto &d = arbitrary_nhood.dims();
        const int H = (int)d.rows();
        const int W = (int)d.cols();
        std::vector<uint8_t> m((size_t)H * (size_t)W, 0);
        for (int c = 0; c < W; ++c)
            for (int r = 0; r < H; ++r) {
                const double v = arbitrary_nhood.elemAsDouble((size_t)c * (size_t)H + (size_t)r);
                m[(size_t)r * (size_t)W + (size_t)c] = (v != 0.0) ? 1 : 0;
            }
        return pack_logical(mr, m, H, W);
    }
    throw Error("strel: unknown shape '" + shape + "'", 0, 0, "strel", "",
                "m:strel:badshape");
}

// ════════════════════════════════════════════════════════════════════
// erode / dilate / open / close
// ════════════════════════════════════════════════════════════════════

namespace {

inline void store_classed_morph(Value &out, size_t i, double v, ValueType t) {
    switch (t) {
        case ValueType::DOUBLE:  out.doubleDataMut()[i]  = v; break;
        case ValueType::SINGLE:  out.singleDataMut()[i]  = (float)v; break;
        case ValueType::UINT8: {
            if (v < 0) v = 0; if (v > 255) v = 255;
            out.uint8DataMut()[i] = (uint8_t)std::lround(v); break;
        }
        case ValueType::UINT16: {
            if (v < 0) v = 0; if (v > 65535) v = 65535;
            out.uint16DataMut()[i] = (uint16_t)std::lround(v); break;
        }
        case ValueType::INT16: {
            if (v < -32768) v = -32768; if (v > 32767) v = 32767;
            out.int16DataMut()[i] = (int16_t)std::lround(v); break;
        }
        case ValueType::LOGICAL:
            out.logicalDataMut()[i] = v != 0.0 ? 1 : 0; break;
        default:
            throw Error("morph: unsupported class", 0, 0, "morph", "",
                        "m:morph:badtype");
    }
}

// Convert SE input to a logical mask (offsets from centre).
struct SEInfo {
    int H, W;
    int half_r, half_c;
    std::vector<uint8_t> mask;  // row-major
};

SEInfo unpack_se(const Value &SE) {
    SEInfo s{};
    s.H = (int)SE.dims().rows();
    s.W = (int)SE.dims().cols();
    s.half_r = s.H / 2;
    s.half_c = s.W / 2;
    s.mask.assign((size_t)s.H * (size_t)s.W, 0);
    for (int c = 0; c < s.W; ++c)
        for (int r = 0; r < s.H; ++r) {
            const double v = SE.elemAsDouble((size_t)c * (size_t)s.H + (size_t)r);
            s.mask[(size_t)r * (size_t)s.W + (size_t)c] = (v != 0.0) ? 1 : 0;
        }
    return s;
}

template <bool IsErode>
Value morph_op(std::pmr::memory_resource *mr, const Value &I, const Value &SE)
{
    auto se = unpack_se(SE);
    const int H = (int)I.dims().rows();
    const int W = (int)I.dims().cols();
    Value out = Value::matrix(H, W, I.type(), mr);
    if (H == 0 || W == 0) return out;

    for (int oc = 0; oc < W; ++oc) {
        for (int orow = 0; orow < H; ++orow) {
            double best = IsErode ?  std::numeric_limits<double>::infinity()
                                  : -std::numeric_limits<double>::infinity();
            for (int kj = 0; kj < se.W; ++kj) {
                const int c_in = oc + kj - se.half_c;
                if (c_in < 0 || c_in >= W) {
                    if (IsErode) {
                        // For binary erode with constant-0 boundary, missing
                        // means failure; for grayscale erode we treat missing
                        // as +∞ (i.e. ignore). MATLAB default = replicate-style
                        // for grayscale; for binary, default is "ignore (i.e.
                        // SE-mask ANDed with image — so out-of-bounds counts
                        // as 0)". We pick simplest: ignore missing, which gives
                        // a valid grayscale erosion with truncated SE.
                    }
                    continue;
                }
                for (int ki = 0; ki < se.H; ++ki) {
                    if (!se.mask[(size_t)ki * (size_t)se.W + (size_t)kj]) continue;
                    const int r_in = orow + ki - se.half_r;
                    if (r_in < 0 || r_in >= H) continue;
                    const double v = I.elemAsDouble((size_t)c_in * (size_t)H + (size_t)r_in);
                    if (IsErode) { if (v < best) best = v; }
                    else         { if (v > best) best = v; }
                }
            }
            if (!std::isfinite(best)) {
                // No SE-marked pixels covered — output value of 0 / class min.
                best = 0.0;
            }
            store_classed_morph(out, (size_t)oc * (size_t)H + (size_t)orow, best, I.type());
        }
    }
    return out;
}

} // anonymous

Value imerode(std::pmr::memory_resource *mr, const Value &I, const Value &SE) {
    return morph_op<true>(mr, I, SE);
}

Value imdilate(std::pmr::memory_resource *mr, const Value &I, const Value &SE) {
    return morph_op<false>(mr, I, SE);
}

Value imopen(std::pmr::memory_resource *mr, const Value &I, const Value &SE) {
    Value e = imerode(mr, I, SE);
    return imdilate(mr, e, SE);
}

Value imclose(std::pmr::memory_resource *mr, const Value &I, const Value &SE) {
    Value d = imdilate(mr, I, SE);
    return imerode(mr, d, SE);
}

// ════════════════════════════════════════════════════════════════════
// imreconstruct — morphological reconstruction by dilation
// ════════════════════════════════════════════════════════════════════
//
// J_{k+1} = min(imdilate(J_k, SE), mask), J_0 = marker.
// The fixed point exists because the iteration is monotone non-
// decreasing (each dilation only adds; the min-cap can never push a
// value above mask). Stopping criterion: J_{k+1} == J_k. Works for
// both binary and grayscale inputs because numkit's `imdilate` is
// the grayscale max-in-neighborhood form (which reduces to binary
// dilation when the input is logical).

Value imreconstruct(std::pmr::memory_resource *mr,
                    const Value &marker, const Value &mask, int conn)
{
    if (conn != 4) conn = 8;
    const size_t H = marker.dims().rows();
    const size_t W = marker.dims().cols();
    if (mask.dims().rows() != H || mask.dims().cols() != W)
        throw Error("imreconstruct: marker and mask must have the same shape",
                    0, 0, "imreconstruct", "", "m:imreconstruct:shape");

    // Build the SE: 3×3 ones for conn=8, plus-shape for conn=4.
    Value SE;
    if (conn == 8) {
        SE = strel(mr, "square",
                   std::vector<double>{3.0},
                   Value::matrix(0, 0, ValueType::DOUBLE, mr));
    } else {
        SE = strel(mr, "diamond",
                   std::vector<double>{1.0},
                   Value::matrix(0, 0, ValueType::DOUBLE, mr));
    }

    const size_t N = marker.numel();
    const ValueType srcT = marker.type();

    // Working buffer J_0 = min(marker, mask) (enforces marker ≤ mask).
    Value J = Value::matrix(H, W, srcT, mr);
    auto writeNative = [&](Value &dst, size_t i, double v) {
        switch (srcT) {
            case ValueType::DOUBLE: dst.doubleDataMut()[i] = v; break;
            case ValueType::SINGLE: dst.singleDataMut()[i] = float(v); break;
            case ValueType::UINT8: {
                if (v < 0.0) v = 0.0; if (v > 255.0) v = 255.0;
                dst.uint8DataMut()[i] = std::uint8_t(std::lround(v)); break;
            }
            case ValueType::UINT16: {
                if (v < 0.0) v = 0.0; if (v > 65535.0) v = 65535.0;
                dst.uint16DataMut()[i] = std::uint16_t(std::lround(v)); break;
            }
            case ValueType::LOGICAL:
                dst.logicalDataMut()[i] = (v != 0.0) ? 1u : 0u; break;
            default:
                dst.doubleDataMut()[i] = v; break;
        }
    };
    for (size_t i = 0; i < N; ++i) {
        const double m = marker.elemAsDouble(i);
        const double k = mask.elemAsDouble(i);
        writeNative(J, i, std::min(m, k));
    }

    // Iterate dilate-and-cap. Bound iterations by min(H,W) — a wave of
    // dilations propagates at most that far.
    const size_t maxIter = static_cast<size_t>(H + W);
    for (size_t iter = 0; iter < maxIter; ++iter) {
        Value Jd = imdilate(mr, J, SE);
        bool changed = false;
        Value Jnew = Value::matrix(H, W, srcT, mr);
        for (size_t i = 0; i < N; ++i) {
            const double cur = J.elemAsDouble(i);
            const double v   = std::min(Jd.elemAsDouble(i),
                                         mask.elemAsDouble(i));
            if (v != cur) changed = true;
            writeNative(Jnew, i, v);
        }
        if (!changed) return J;
        J = std::move(Jnew);
    }
    return J;
}

// ════════════════════════════════════════════════════════════════════
// imfill('holes')  — composes imreconstruct
// ════════════════════════════════════════════════════════════════════
//
// A "hole" is a 0-pixel NOT connectivity-reachable from the image
// border. We reconstruct the border-touching part of the complement
// (background reachable from the rim) through ~BW, then take the
// complement of that result — anything that wasn't reachable from
// the border becomes 1 in the output, which means foreground
// pixels stay 1 and holes are filled.

Value imfill_holes(std::pmr::memory_resource *mr,
                   const Value &BW, int conn)
{
    if (conn != 4) conn = 8;
    const size_t H = BW.dims().rows();
    const size_t W = BW.dims().cols();
    Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;

    // marker = ~BW restricted to the image border (rim).
    // mask    = ~BW.
    Value marker = Value::matrix(H, W, ValueType::LOGICAL, mr);
    Value mask   = Value::matrix(H, W, ValueType::LOGICAL, mr);
    std::uint8_t *md = marker.logicalDataMut();
    std::uint8_t *kd = mask.logicalDataMut();
    for (size_t c = 0; c < W; ++c)
        for (size_t r = 0; r < H; ++r) {
            const size_t idx = c * H + r;
            const bool fg = (BW.elemAsDouble(idx) != 0.0);
            kd[idx] = fg ? 0u : 1u;            // ~BW
            const bool onRim = (r == 0 || c == 0 ||
                                  r + 1 == H || c + 1 == W);
            md[idx] = (onRim && !fg) ? 1u : 0u; // marker = rim ∩ ~BW
        }

    Value R = imreconstruct(mr, marker, mask, conn);

    // J = ~R.
    std::uint8_t *od = out.logicalDataMut();
    for (size_t i = 0; i < H * W; ++i) {
        const bool r = (R.elemAsDouble(i) != 0.0);
        od[i] = r ? 0u : 1u;
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// imregionalmax / imregionalmin
// ════════════════════════════════════════════════════════════════════
//
// Standard formula:  regmax(I) = (I − imreconstruct(I − 1, I)) > 0.
// The marker (I − 1) is below I except at regional maxima, where the
// reconstruction can't grow back up to I (because dilation only takes
// from neighbours that are themselves capped at I − 1). So pixels
// where the reconstruction equals I lie outside any regional max,
// and pixels where it stays below I are exactly the maxima.
//
// imregionalmin reuses imregionalmax on a value-inverted copy of I:
//   typeMax − I    for unsigned integer classes
//   − I             for signed / floating-point
// Inverting flips peaks↔troughs, so regional maxima of −I are the
// regional minima of I.

Value imregionalmax(std::pmr::memory_resource *mr,
                    const Value &I, int conn)
{
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    const size_t N = I.numel();
    Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
    if (N == 0) return out;

    // Build the marker = max(I − 1, lower_bound). Use DOUBLE through
    // the operation to avoid the integer-saturation surprise for
    // I = 0 / I = INT_MIN / etc.
    Value marker = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value mask   = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *md = marker.doubleDataMut();
    double *kd = mask.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double v = I.elemAsDouble(i);
        kd[i] = v;
        md[i] = v - 1.0;
    }
    Value R = imreconstruct(mr, marker, mask, conn);

    // Output = (I > R).
    std::uint8_t *od = out.logicalDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double iv = I.elemAsDouble(i);
        const double rv = R.elemAsDouble(i);
        od[i] = (iv > rv) ? 1u : 0u;
    }
    return out;
}

Value imregionalmin(std::pmr::memory_resource *mr,
                    const Value &I, int conn)
{
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    const size_t N = I.numel();
    Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
    if (N == 0) return out;

    // Invert into a DOUBLE copy: (high - I) flips the order so peaks
    // become troughs. high is chosen large enough that I_inv stays
    // non-negative for unsigned classes (purely cosmetic — only the
    // ordering matters).
    double high = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double v = I.elemAsDouble(i);
        if (v > high) high = v;
    }
    Value Iinv = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *id = Iinv.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        id[i] = high - I.elemAsDouble(i);

    return imregionalmax(mr, Iinv, conn);
}

// ════════════════════════════════════════════════════════════════════
// imhmax / imhmin — h-extrema transforms
// ════════════════════════════════════════════════════════════════════
//
//   imhmax(I, h) = imreconstruct(I − h, I)
//   imhmin(I, h) = invert(imhmax(invert(I), h))
//
// h-maxima suppresses regional maxima shallower than h. Used as a
// precursor to imregionalmax to ignore small / noise-like peaks.

Value imhmax(std::pmr::memory_resource *mr,
             const Value &I, double h, int conn)
{
    if (!(h >= 0.0))
        throw Error("imhmax: h must be ≥ 0",
                    0, 0, "imhmax", "", "m:imhmax:h");
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    const size_t N = I.numel();
    if (N == 0) return Value::matrix(H, W, ValueType::DOUBLE, mr);

    Value marker = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value mask   = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *md = marker.doubleDataMut();
    double *kd = mask.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double v = I.elemAsDouble(i);
        kd[i] = v;
        md[i] = v - h;
    }
    return imreconstruct(mr, marker, mask, conn);
}

Value imhmin(std::pmr::memory_resource *mr,
             const Value &I, double h, int conn)
{
    if (!(h >= 0.0))
        throw Error("imhmin: h must be ≥ 0",
                    0, 0, "imhmin", "", "m:imhmin:h");
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    const size_t N = I.numel();
    if (N == 0) return Value::matrix(H, W, ValueType::DOUBLE, mr);

    // Invert about a high value so peaks ↔ troughs, then reuse imhmax,
    // then invert back.
    double high = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double v = I.elemAsDouble(i);
        if (v > high) high = v;
    }
    Value Iinv = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *id = Iinv.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        id[i] = high - I.elemAsDouble(i);

    Value Jinv = imhmax(mr, Iinv, h, conn);

    // Invert back: J = high - Jinv.
    Value J = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *jd = J.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        jd[i] = high - Jinv.elemAsDouble(i);
    return J;
}

// ════════════════════════════════════════════════════════════════════
// imextendedmax / imextendedmin — extended-extrema transforms
// ════════════════════════════════════════════════════════════════════
//
//   imextendedmax(I, h) = imregionalmax(imhmax(I, h))
//   imextendedmin(I, h) = imregionalmin(imhmin(I, h))
//
// First flatten any peak shallower than h (imhmax), then locate the
// regional maxima of the result. Pixels in the output are exactly
// those that belong to a regional maximum at least h units above its
// surroundings — the "tall enough" peaks.

Value imextendedmax(std::pmr::memory_resource *mr,
                    const Value &I, double h, int conn)
{
    return imregionalmax(mr, imhmax(mr, I, h, conn), conn);
}

Value imextendedmin(std::pmr::memory_resource *mr,
                    const Value &I, double h, int conn)
{
    return imregionalmin(mr, imhmin(mr, I, h, conn), conn);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void strel_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strel: requires shape", 0, 0, "strel", "",
                    "m:strel:nargin");
    std::string shape = "square";
    if (args[0].isChar() || args[0].isString()) shape = args[0].toString();
    std::vector<double> params;
    Value arbitrary;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!(args[i].isChar() || args[i].isString())) {
            if (shape == "arbitrary") { arbitrary = args[i]; continue; }
            for (size_t j = 0; j < args[i].numel(); ++j)
                params.push_back(args[i].elemAsDouble(j));
        }
    }
    outs[0] = strel(ctx.engine->resource(), shape, params, arbitrary);
}

#define NK_MORPH_REG(name)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                 \
                    Span<Value> outs, CallContext &ctx)                         \
    {                                                                             \
        if (args.size() < 2)                                                      \
            throw Error(#name ": requires (I, SE)", 0, 0, #name, "",             \
                        "m:" #name ":nargin");                                   \
        outs[0] = name(ctx.engine->resource(), args[0], args[1]);                \
    }

NK_MORPH_REG(imerode)
NK_MORPH_REG(imdilate)
NK_MORPH_REG(imopen)
NK_MORPH_REG(imclose)

#undef NK_MORPH_REG

void imreconstruct_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imreconstruct: requires (marker, mask [, conn])",
                    0, 0, "imreconstruct", "", "m:imreconstruct:nargin");
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imreconstruct(ctx.engine->resource(),
                            args[0], args[1], conn);
}

void imfill_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imfill: requires (BW, 'holes' [, conn])",
                    0, 0, "imfill", "", "m:imfill:nargin");
    auto *mr = ctx.engine->resource();
    // Currently we support `imfill(BW, 'holes' [, conn])` only.
    if (args.size() < 2 ||
        !(args[1].isChar() || args[1].isString()))
        throw Error("imfill: only the 'holes' mode is implemented",
                    0, 0, "imfill", "", "m:imfill:mode");
    const std::string mode = args[1].toString();
    if (mode != "holes" && mode != "Holes" && mode != "HOLES")
        throw Error("imfill: only 'holes' mode is implemented "
                    "(seed-list mode not yet supported)",
                    0, 0, "imfill", "", "m:imfill:mode");
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imfill_holes(mr, args[0], conn);
}

void imregionalmax_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imregionalmax: requires (I [, conn])",
                    0, 0, "imregionalmax", "", "m:imregionalmax:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? static_cast<int>(args[1].toScalar()) : 8;
    outs[0] = imregionalmax(ctx.engine->resource(), args[0], conn);
}

void imregionalmin_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imregionalmin: requires (I [, conn])",
                    0, 0, "imregionalmin", "", "m:imregionalmin:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? static_cast<int>(args[1].toScalar()) : 8;
    outs[0] = imregionalmin(ctx.engine->resource(), args[0], conn);
}

void imhmax_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imhmax: requires (I, h [, conn])",
                    0, 0, "imhmax", "", "m:imhmax:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imhmax(ctx.engine->resource(), args[0], h, conn);
}

void imhmin_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imhmin: requires (I, h [, conn])",
                    0, 0, "imhmin", "", "m:imhmin:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imhmin(ctx.engine->resource(), args[0], h, conn);
}

void imextendedmax_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imextendedmax: requires (I, h [, conn])",
                    0, 0, "imextendedmax", "", "m:imextendedmax:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imextendedmax(ctx.engine->resource(), args[0], h, conn);
}

void imextendedmin_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imextendedmin: requires (I, h [, conn])",
                    0, 0, "imextendedmin", "", "m:imextendedmin:nargin");
    const double h = args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? static_cast<int>(args[2].toScalar()) : 8;
    outs[0] = imextendedmin(ctx.engine->resource(), args[0], h, conn);
}

} // namespace detail
} // namespace numkit::image
