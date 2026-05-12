// libs/image/src/color/color_extras.cpp
//
// Image Toolbox color-conversion extras (cycle 3 of the sweep):
//
//   rgb2lightness(RGB)             RGB → L (lightness, L* of CIE Lab).
//                                  Returns H×W single image.
//   [X, cmap] = rgb2ind(RGB, inmap) Quantize to fixed colormap by
//                                  nearest-neighbour search in RGB.
//                                  X is uint8/uint16 (1-based index;
//                                  uint8 if cmap rows ≤ 256, otherwise
//                                  uint16). cmap echoed unchanged.
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.
//
// KNOWN GAPs (rgb2ind):
//   * `rgb2ind(RGB, Q)` minimum-variance quantization (median-cut style)
//     — deferred (~150 LOC algorithm work).
//   * `rgb2ind(RGB, tol)` uniform-grid quantization — deferred.
//   * `dithering` arg ('dither' | 'nodither') — deferred; always
//     equivalent to 'nodither' (no error diffusion).

#include <numkit/image/color/color.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace numkit::image {

// ── rgb2lightness ─────────────────────────────────────────────────────
// L = first channel of rgb2lab(RGB). MATLAB's rgb2lightness returns
// single. Numkit's rgb2lab returns double, so we cast on the way out.
Value rgb2lightness(const Value &RGB, std::pmr::memory_resource *mr)
{
    if (!RGB.dims().is3D() || RGB.dims().pages() != 3)
        throw Error("rgb2lightness: input must be H×W×3",
                    0, 0, "rgb2lightness", "", "m:rgb2lightness:Shape");
    Value lab = rgb2lab(RGB, mr);
    const size_t H = lab.dims().rows();
    const size_t W = lab.dims().cols();
    Value out = Value::matrix(H, W, ValueType::SINGLE, mr);
    if (H == 0 || W == 0) return out;
    // Page 0 of column-major H×W×3 = first H*W elements.
    float *dst = out.singleDataMut();
    if (lab.type() == ValueType::SINGLE) {
        std::memcpy(dst, lab.singleData(), H * W * sizeof(float));
    } else if (lab.type() == ValueType::DOUBLE) {
        const double *src = lab.doubleData();
        for (size_t i = 0; i < H * W; ++i) dst[i] = static_cast<float>(src[i]);
    } else {
        // Fallback via elemAsDouble for any other lab type.
        for (size_t i = 0; i < H * W; ++i)
            dst[i] = static_cast<float>(lab.elemAsDouble(i));
    }
    return out;
}

// ── rgb2ind ───────────────────────────────────────────────────────────
// Fixed-palette form only in v1: nearest-RGB quantization (no dithering).
//   X(i, j) = argmin_k ||RGB(i, j, :) - cmap(k, :)||^2   (0-based!)
// Output index class: uint8 if cmap rows ≤ 256, else uint16. MATLAB
// returns 0-based indices for integer output classes (1-based only when
// the output is double, which our v1 doesn't produce). Tie-breaking
// picks the lowest cmap index (matches MATLAB's nodither path).
std::pair<Value, Value>
rgb2ind_inmap(const Value &RGB, const Value &cmap, std::pmr::memory_resource *mr)
{
    if (!RGB.dims().is3D() || RGB.dims().pages() != 3)
        throw Error("rgb2ind: input must be H×W×3",
                    0, 0, "rgb2ind", "", "m:rgb2ind:Shape");
    if (cmap.dims().is3D() || cmap.dims().cols() != 3)
        throw Error("rgb2ind: colormap must be K×3",
                    0, 0, "rgb2ind", "", "m:rgb2ind:CmapShape");

    const size_t H = RGB.dims().rows();
    const size_t W = RGB.dims().cols();
    const size_t K = cmap.dims().rows();
    const size_t HW = H * W;

    // Convert RGB pixels to double in [0, 1].
    ScratchArena scratch(mr);
    ScratchVec<double> pix(HW * 3, &scratch);
    {
        const double scale = (RGB.type() == ValueType::UINT8)  ? 1.0 / 255.0
                            : (RGB.type() == ValueType::UINT16) ? 1.0 / 65535.0
                            : 1.0;  // double/single already in [0,1]
        for (size_t p = 0; p < 3; ++p)
            for (size_t i = 0; i < HW; ++i)
                pix[p * HW + i] = RGB.elemAsDouble(p * HW + i) * scale;
    }

    // Decide output index class.
    const ValueType idxT = (K <= 256) ? ValueType::UINT8 : ValueType::UINT16;
    Value X = Value::matrix(H, W, idxT, mr);
    if (HW == 0) return {X, cmap};

    // Read cmap as K×3.
    ScratchVec<double> cm(K * 3, &scratch);
    for (size_t c = 0; c < 3; ++c)
        for (size_t k = 0; k < K; ++k)
            cm[c * K + k] = cmap.elemAsDouble(k + c * K);

    auto write_idx = [&](size_t i, size_t k1based) {
        if (idxT == ValueType::UINT8)
            X.uint8DataMut()[i] = static_cast<uint8_t>(k1based);
        else
            X.uint16DataMut()[i] = static_cast<uint16_t>(k1based);
    };

    for (size_t i = 0; i < HW; ++i) {
        const double r = pix[0 * HW + i];
        const double g = pix[1 * HW + i];
        const double b = pix[2 * HW + i];
        double bestDist = std::numeric_limits<double>::infinity();
        size_t bestK = 0;
        for (size_t k = 0; k < K; ++k) {
            const double dr = r - cm[0 * K + k];
            const double dg = g - cm[1 * K + k];
            const double db = b - cm[2 * K + k];
            const double d2 = dr * dr + dg * dg + db * db;
            if (d2 < bestDist) { bestDist = d2; bestK = k; }
        }
        write_idx(i, bestK);  // 0-based (matches MATLAB uint8/uint16 output)
    }
    return {X, cmap};
}

namespace detail {

void rgb2lightness_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rgb2lightness: requires (RGB)",
                    0, 0, "rgb2lightness", "", "m:rgb2lightness:nargin");
    outs[0] = rgb2lightness(args[0], ctx.engine->resource());
}

void rgb2ind_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgb2ind: requires (RGB, inmap [, dithering]) — "
                    "Q/tol forms deferred",
                    0, 0, "rgb2ind", "", "m:rgb2ind:nargin");
    // Optional 3rd arg: 'dither' (default in MATLAB) | 'nodither'.
    // Numkit always behaves as 'nodither'; if 'dither' is requested we
    // throw to keep parity honest (KNOWN GAP).
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
        const std::string s = args[2].toString();
        if (s == "dither")
            throw Error("rgb2ind: 'dither' option not implemented in v1 "
                        "(KNOWN GAP); pass 'nodither' instead",
                        0, 0, "rgb2ind", "", "m:rgb2ind:NoDither");
        if (s != "nodither")
            throw Error("rgb2ind: dithering arg must be 'dither' or 'nodither'",
                        0, 0, "rgb2ind", "", "m:rgb2ind:BadOpt");
    }
    // Q (positive integer scalar) and tol (real in [0,1]) forms throw.
    if (args[1].numel() == 1) {
        throw Error("rgb2ind: scalar Q (min-variance quant) and tol "
                    "(uniform quant) forms not implemented in v1; pass "
                    "an explicit K×3 colormap instead (KNOWN GAP)",
                    0, 0, "rgb2ind", "", "m:rgb2ind:NotImpl");
    }
    auto [X, cmap] = rgb2ind_inmap(args[0], args[1], ctx.engine->resource());
    outs[0] = X;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = cmap;
}

} // namespace detail

} // namespace numkit::image
