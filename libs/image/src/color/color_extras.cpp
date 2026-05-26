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

// ── rgbwide2ycbcr (BT.2020 / BT.2100 narrow-range) ─────────────────
//
// Reference: ITU-R Rec. BT.2020-2 (10/2015) and BT.2100-2 (07/2018)
// — non-constant-luminance YCbCr encoding for wide-gamut UHDTV /
// HDR-TV. Algorithm transliterated verbatim from MATLAB R2025b
// `images/colorspaces/+images/+color/+internal/rgbwide2ycbcrImpl.m`
// (column-major / non-codegen path).
//
// Pipeline (per pixel):
//   1. blackLevel    = 64 (bps = 10) or 256 (bps = 12)
//      nominalPeak   = 940       or 3760
//      nominalRange  = peak − black
//   2. R_n = (R − black)/range, G_n same, B_n same.
//   3. Y'  = 0.2627·R_n + 0.6780·G_n + 0.0593·B_n   (BT.2020 luma).
//   4. Cb  = (B_n − Y')/1.8814,  Cr = (R_n − Y')/1.4746.
//   5. Quantise (uint16):
//        Y_out  = round((219·Y'  + 16 )·2^(bps − 8))
//        Cb_out = round((224·Cb + 128)·2^(bps − 8))
//        Cr_out = round((224·Cr + 128)·2^(bps − 8))
Value rgbwide2ycbcr(const Value &RGB, int bits_per_sample,
                    std::pmr::memory_resource *mr)
{
    if (bits_per_sample != 10 && bits_per_sample != 12)
        throw Error("rgbwide2ycbcr: BPS must be 10 or 12",
                    0, 0, "rgbwide2ycbcr", "", "m:rgbwide2ycbcr:bps");
    if (RGB.type() != ValueType::UINT16)
        throw Error("rgbwide2ycbcr: RGB must be UINT16",
                    0, 0, "rgbwide2ycbcr", "", "m:rgbwide2ycbcr:class");

    const auto &d = RGB.dims();
    // Two shapes: p × 3 colour list, or H × W × 3 image.
    const bool is_image = d.is3D();
    if (is_image) {
        if (d.pages() != 3)
            throw Error("rgbwide2ycbcr: H×W×3 image expected",
                        0, 0, "rgbwide2ycbcr", "", "m:rgbwide2ycbcr:shape");
    } else {
        if (d.cols() != 3)
            throw Error("rgbwide2ycbcr: p×3 colour list expected",
                        0, 0, "rgbwide2ycbcr", "", "m:rgbwide2ycbcr:shape");
    }

    const double blackLevel  = (bits_per_sample == 10) ?  64.0 : 256.0;
    const double nominalPeak = (bits_per_sample == 10) ? 940.0 : 3760.0;
    const double nominalRange = nominalPeak - blackLevel;
    const double scale        = (bits_per_sample == 10) ?  4.0 :  16.0; // 2^(bps-8)

    const std::size_t H = is_image ? d.rows() : d.rows();
    const std::size_t W = is_image ? d.cols() : 1;
    const std::size_t N = H * W;

    Value out;
    if (is_image) out = Value::matrix3d(H, W, 3, ValueType::UINT16, mr);
    else          out = Value::matrix(H, 3, ValueType::UINT16, mr);

    const uint16_t *src = RGB.uint16Data();
    uint16_t *dst = out.uint16DataMut();

    // For the p × 3 list, channels are columns (stride = H = p).
    // For the H × W × 3 image, channels are pages (stride = H · W = N).
    const std::size_t ch_stride = is_image ? N : H;

    auto clamp_round = [](double v) -> uint16_t {
        if (v < 0.0)        return 0;
        if (v > 65535.0)    return 65535;
        return static_cast<uint16_t>(v + 0.5);   // round-half-up
    };

    for (std::size_t k = 0; k < (is_image ? N : H); ++k) {
        const double R = (static_cast<double>(src[0 * ch_stride + k]) - blackLevel) / nominalRange;
        const double G = (static_cast<double>(src[1 * ch_stride + k]) - blackLevel) / nominalRange;
        const double B = (static_cast<double>(src[2 * ch_stride + k]) - blackLevel) / nominalRange;
        const double Y  = 0.2627 * R + 0.6780 * G + 0.0593 * B;
        const double Cb = (B - Y) / 1.8814;
        const double Cr = (R - Y) / 1.4746;
        dst[0 * ch_stride + k] = clamp_round((219.0 * Y  +  16.0) * scale);
        dst[1 * ch_stride + k] = clamp_round((224.0 * Cb + 128.0) * scale);
        dst[2 * ch_stride + k] = clamp_round((224.0 * Cr + 128.0) * scale);
    }
    return out;
}

namespace detail {

void rgbwide2ycbcr_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgbwide2ycbcr: requires (RGB, BPS)",
                    0, 0, "rgbwide2ycbcr", "", "m:rgbwide2ycbcr:nargin");
    const int bps = static_cast<int>(args[1].toScalar());
    outs[0] = rgbwide2ycbcr(args[0], bps, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
