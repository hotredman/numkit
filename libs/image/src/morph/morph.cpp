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

} // namespace detail
} // namespace numkit::image
