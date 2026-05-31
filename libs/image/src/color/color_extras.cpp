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
#include <numkit/image/geom/geom.hpp>
#include <numkit/image/contrast/contrast.hpp>

#include <numkit/builtin/math/random/matlab_mt19937.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace numkit::image {

// ── rgb2lightness ─────────────────────────────────────────────────────
// L = first channel of rgb2lab(RGB). MATLAB's rgb2lightness returns
// single. Numkit's rgb2lab returns double, so we cast on the way out.
Value rgb2lightness(const Value &RGB, std::pmr::memory_resource *mr)
{
    if (!RGB.dims().is3D() || RGB.dims().pages() != 3)
        throw Error("rgb2lightness: input must be H×W×3",
                    0, 0, "rgb2lightness", "", "numkit:rgb2lightness:Shape");
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
                    0, 0, "rgb2ind", "", "numkit:rgb2ind:Shape");
    if (cmap.dims().is3D() || cmap.dims().cols() != 3)
        throw Error("rgb2ind: colormap must be K×3",
                    0, 0, "rgb2ind", "", "numkit:rgb2ind:CmapShape");

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
                    0, 0, "rgb2lightness", "", "numkit:rgb2lightness:nargin");
    outs[0] = rgb2lightness(args[0], ctx.engine->resource());
}

void rgb2ind_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgb2ind: requires (RGB, inmap [, dithering]) — "
                    "Q/tol forms deferred",
                    0, 0, "rgb2ind", "", "numkit:rgb2ind:nargin");
    // Optional 3rd arg: 'dither' (default in MATLAB) | 'nodither'.
    // Numkit always behaves as 'nodither'; if 'dither' is requested we
    // throw to keep parity honest (KNOWN GAP).
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
        const std::string s = args[2].toString();
        if (s == "dither")
            throw Error("rgb2ind: 'dither' option not implemented in v1 "
                        "(KNOWN GAP); pass 'nodither' instead",
                        0, 0, "rgb2ind", "", "numkit:rgb2ind:NoDither");
        if (s != "nodither")
            throw Error("rgb2ind: dithering arg must be 'dither' or 'nodither'",
                        0, 0, "rgb2ind", "", "numkit:rgb2ind:BadOpt");
    }
    // Q (positive integer scalar) and tol (real in [0,1]) forms throw.
    if (args[1].numel() == 1) {
        throw Error("rgb2ind: scalar Q (min-variance quant) and tol "
                    "(uniform quant) forms not implemented in v1; pass "
                    "an explicit K×3 colormap instead (KNOWN GAP)",
                    0, 0, "rgb2ind", "", "numkit:rgb2ind:NotImpl");
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
                    0, 0, "rgbwide2ycbcr", "", "numkit:rgbwide2ycbcr:bps");
    if (RGB.type() != ValueType::UINT16)
        throw Error("rgbwide2ycbcr: RGB must be UINT16",
                    0, 0, "rgbwide2ycbcr", "", "numkit:rgbwide2ycbcr:class");

    const auto &d = RGB.dims();
    // Two shapes: p × 3 colour list, or H × W × 3 image.
    const bool is_image = d.is3D();
    if (is_image) {
        if (d.pages() != 3)
            throw Error("rgbwide2ycbcr: H×W×3 image expected",
                        0, 0, "rgbwide2ycbcr", "", "numkit:rgbwide2ycbcr:shape");
    } else {
        if (d.cols() != 3)
            throw Error("rgbwide2ycbcr: p×3 colour list expected",
                        0, 0, "rgbwide2ycbcr", "", "numkit:rgbwide2ycbcr:shape");
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

// ── cmunique (remove duplicate colormap entries) ───────────────────
//
// MATLAB R2025b cmunique.m algorithm (transliterated verbatim):
//
//   tol = 1/1024
//   map  = round(map / tol) * tol                — quantise
//   [~, ndx] = sortrows(map, [3 2 1])             — sort by (B, G, R)
//   pos(ndx) = 1:length(ndx)                      — inverse perm
//   d  = all(abs(diff(map(ndx,:)))' < tol)'       — consecutive dup
//   loc = (1:length(ndx))' - [0; cumsum(d)]       — sorted→compressed
//   c(:) = loc(pos(c))                            — remap indices
//   ndx(d) = []                                   — drop dup rows
//   map = map(ndx, :)                             — compressed map
//   n  = histcounts(c, 1:nmap+1)
//   d  = (n == 0)                                  — unused rows
//   loc = (1:nmap) - cumsum(d)                    — compressed→final
//   c(:) = loc(c)
//   map(d, :) = []
//   if max(c) ≤ 256: c = uint8(c - 1)             — 0-based output
//
// For the (RGB) and (I) one-arg forms, build the per-pixel "big
// colormap" (1 row per pixel, im2double-converted) and `c =
// reshape(1:m*n, m, n)` first, then run the same compression pass.
//
// Reference: see MATLAB toolbox source
// `toolbox/matlab/graphics/graphics/graph3d/cmunique.m`.

namespace {

// Convert input pixel value (any class) to double in [0, 1] per
// MATLAB's im2double rules (uint8 / uint16 scaled by intmax; double
// and single passed through).
inline double im2double_scalar(const Value &v, std::size_t i)
{
    switch (v.type()) {
        case ValueType::UINT8:  return v.uint8Data()[i] / 255.0;
        case ValueType::UINT16: return v.uint16Data()[i] / 65535.0;
        case ValueType::DOUBLE: return v.doubleData()[i];
        case ValueType::SINGLE: return static_cast<double>(v.singleData()[i]);
        default:
            throw Error("cmunique: unsupported image class",
                        0, 0, "cmunique", "", "numkit:cmunique:cls");
    }
}

// Core compression pass. `c` is a 1-based double index vector of
// length N referencing `map_in` (M × 3 colormap). Outputs `(Y,
// newmap)` per MATLAB rules — Y is uint8 if newmap has ≤ 256 rows.
std::pair<Value, Value>
cmunique_core(std::pmr::vector<double> &c, std::size_t H, std::size_t W,
              const std::pmr::vector<double> &map_in, std::size_t M,
              std::pmr::memory_resource *mr)
{
    constexpr double tol = 1.0 / 1024.0;

    // Quantise map.
    std::pmr::vector<double> map(3 * M, mr);
    for (std::size_t k = 0; k < 3 * M; ++k)
        map[k] = std::round(map_in[k] / tol) * tol;

    // Sort indices by columns [3 2 1] ascending. map is col-major
    // M × 3: map[r, c] = map_in[c * M + r].
    std::pmr::vector<std::size_t> ndx(M, mr);
    for (std::size_t k = 0; k < M; ++k) ndx[k] = k;
    std::sort(ndx.begin(), ndx.end(),
        [&](std::size_t a, std::size_t b) {
            const double a3 = map[2 * M + a], b3 = map[2 * M + b];
            if (a3 != b3) return a3 < b3;
            const double a2 = map[1 * M + a], b2 = map[1 * M + b];
            if (a2 != b2) return a2 < b2;
            return map[0 * M + a] < map[0 * M + b];
        });

    // pos[ndx[k]] = k+1 (1-based).
    std::pmr::vector<std::size_t> pos(M, mr);
    for (std::size_t k = 0; k < M; ++k) pos[ndx[k]] = k + 1;

    // d[k] = (consecutive sorted rows k and k+1 match within tol).
    std::pmr::vector<unsigned char> d(M > 0 ? M - 1 : 0, 0, mr);
    for (std::size_t k = 0; k + 1 < M; ++k) {
        const std::size_t a = ndx[k], b = ndx[k + 1];
        const bool same =
            std::fabs(map[0 * M + a] - map[0 * M + b]) < tol &&
            std::fabs(map[1 * M + a] - map[1 * M + b]) < tol &&
            std::fabs(map[2 * M + a] - map[2 * M + b]) < tol;
        d[k] = same ? 1 : 0;
    }

    // loc[k] = (k+1) - cumsum(d)[k]  with prefix [0; cumsum(d)].
    std::pmr::vector<std::size_t> loc(M, mr);
    std::size_t csum = 0;
    for (std::size_t k = 0; k < M; ++k) {
        loc[k] = (k + 1) - csum;
        if (k < d.size()) csum += d[k];
    }

    // Remap c: c[i] = loc[pos[c[i] - 1] - 1].  (Both pos and loc
    // are 1-based on output; we convert input c to 0-based then
    // back.)
    const std::size_t N = c.size();
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t orig = static_cast<std::size_t>(c[i]) - 1;
        const std::size_t sorted_pos = pos[orig] - 1;
        c[i] = static_cast<double>(loc[sorted_pos]);
    }

    // Drop duplicate rows from ndx (positions where d is true).
    std::pmr::vector<std::size_t> kept_ndx(mr);
    kept_ndx.reserve(M);
    for (std::size_t k = 0; k < M; ++k) {
        const bool drop = (k < d.size()) && d[k];
        if (!drop) kept_ndx.push_back(ndx[k]);
    }
    const std::size_t nmap1 = kept_ndx.size();

    // Build compressed map.
    std::pmr::vector<double> map1(3 * nmap1, mr);
    for (std::size_t k = 0; k < nmap1; ++k) {
        map1[0 * nmap1 + k] = map[0 * M + kept_ndx[k]];
        map1[1 * nmap1 + k] = map[1 * M + kept_ndx[k]];
        map1[2 * nmap1 + k] = map[2 * M + kept_ndx[k]];
    }

    // Count usage of each compressed colour.
    std::pmr::vector<std::size_t> nuse(nmap1, 0, mr);
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t k = static_cast<std::size_t>(c[i]);
        if (k >= 1 && k <= nmap1) ++nuse[k - 1];
    }
    // d2[k] = (nuse[k] == 0). Remap unused → drop.
    std::pmr::vector<unsigned char> d2(nmap1, 0, mr);
    for (std::size_t k = 0; k < nmap1; ++k) d2[k] = (nuse[k] == 0) ? 1 : 0;
    // loc2[k] = (k+1) - cumsum(d2)[k+1].
    std::pmr::vector<std::size_t> loc2(nmap1, mr);
    std::size_t cs = 0;
    for (std::size_t k = 0; k < nmap1; ++k) {
        cs += d2[k];
        loc2[k] = (k + 1) - cs;
    }
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t k = static_cast<std::size_t>(c[i]) - 1;
        c[i] = static_cast<double>(loc2[k]);
    }
    // Remove unused rows from map1.
    std::pmr::vector<double> map2_R(mr), map2_G(mr), map2_B(mr);
    for (std::size_t k = 0; k < nmap1; ++k) {
        if (!d2[k]) {
            map2_R.push_back(map1[0 * nmap1 + k]);
            map2_G.push_back(map1[1 * nmap1 + k]);
            map2_B.push_back(map1[2 * nmap1 + k]);
        }
    }
    const std::size_t nmap2 = map2_R.size();

    // Determine final output class for Y.
    double max_c = 0.0;
    for (std::size_t i = 0; i < N; ++i) if (c[i] > max_c) max_c = c[i];
    const bool to_uint8 = (max_c <= 256.0);

    Value Y = to_uint8
        ? Value::matrix(H, W, ValueType::UINT8, mr)
        : Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (to_uint8) {
        std::uint8_t *yp = Y.uint8DataMut();
        for (std::size_t i = 0; i < N; ++i)
            yp[i] = static_cast<std::uint8_t>(c[i] - 1.0);   // 0-based
    } else {
        double *yp = Y.doubleDataMut();
        for (std::size_t i = 0; i < N; ++i) yp[i] = c[i];     // 1-based
    }

    Value newmap = Value::matrix(nmap2, 3, ValueType::DOUBLE, mr);
    double *np = newmap.doubleDataMut();
    for (std::size_t k = 0; k < nmap2; ++k) {
        np[0 * nmap2 + k] = map2_R[k];
        np[1 * nmap2 + k] = map2_G[k];
        np[2 * nmap2 + k] = map2_B[k];
    }
    return {std::move(Y), std::move(newmap)};
}

} // anonymous

std::pair<Value, Value>
cmunique_xm(const Value &X, const Value &MAP, std::pmr::memory_resource *mr)
{
    if (MAP.dims().cols() != 3 || MAP.dims().is3D())
        throw Error("cmunique: MAP must be N×3",
                    0, 0, "cmunique", "", "numkit:cmunique:map");
    const std::size_t M = MAP.dims().rows();
    const std::size_t H = X.dims().rows();
    const std::size_t W = X.dims().cols();
    const std::size_t N = X.numel();

    std::pmr::vector<double> map_in(3 * M, mr);
    for (std::size_t c = 0; c < 3; ++c)
        for (std::size_t r = 0; r < M; ++r)
            map_in[c * M + r] = MAP.elemAsDouble(c * M + r);

    // Convert X to double + offset for integer classes (MATLAB:
    // c = double(a) + 1 for non-double X).
    std::pmr::vector<double> c(N, mr);
    const bool isInt = (X.type() == ValueType::UINT8
                     || X.type() == ValueType::UINT16);
    for (std::size_t i = 0; i < N; ++i)
        c[i] = X.elemAsDouble(i) + (isInt ? 1.0 : 0.0);

    return cmunique_core(c, H, W, map_in, M, mr);
}

std::pair<Value, Value>
cmunique_rgb(const Value &RGB, std::pmr::memory_resource *mr)
{
    if (!RGB.dims().is3D() || RGB.dims().pages() != 3)
        throw Error("cmunique: RGB must be H×W×3",
                    0, 0, "cmunique", "", "numkit:cmunique:rgb");
    const std::size_t H = RGB.dims().rows();
    const std::size_t W = RGB.dims().cols();
    const std::size_t N = H * W;
    const std::size_t M = N;
    std::pmr::vector<double> map_in(3 * M, mr);
    // map(:, c) = im2double(reshape(RGB(:,:,c), [], 1))
    for (std::size_t c = 0; c < 3; ++c)
        for (std::size_t k = 0; k < N; ++k)
            map_in[c * M + k] = im2double_scalar(RGB, c * N + k);
    // c = reshape(1:N, H, W) — column-major identity 1..N.
    std::pmr::vector<double> idx(N, mr);
    for (std::size_t i = 0; i < N; ++i) idx[i] = static_cast<double>(i + 1);
    return cmunique_core(idx, H, W, map_in, M, mr);
}

std::pair<Value, Value>
cmunique_i(const Value &I, std::pmr::memory_resource *mr)
{
    if (I.dims().ndims() > 2)
        throw Error("cmunique: I must be a 2-D intensity image",
                    0, 0, "cmunique", "", "numkit:cmunique:i");
    const std::size_t H = I.dims().rows();
    const std::size_t W = I.dims().cols();
    const std::size_t N = H * W;
    const std::size_t M = N;
    std::pmr::vector<double> map_in(3 * M, mr);
    for (std::size_t k = 0; k < N; ++k) {
        const double v = im2double_scalar(I, k);
        map_in[0 * M + k] = v;
        map_in[1 * M + k] = v;
        map_in[2 * M + k] = v;
    }
    std::pmr::vector<double> idx(N, mr);
    for (std::size_t i = 0; i < N; ++i) idx[i] = static_cast<double>(i + 1);
    return cmunique_core(idx, H, W, map_in, M, mr);
}

// ── ycbcr2rgbwide — inverse of rgbwide2ycbcr ───────────────────────
//
// Reference: ITU-R Rec. BT.2020-2 (10/2015) and BT.2100-2 (07/2018).
// Algorithm transliterated verbatim from MATLAB R2025b
// `images/colorspaces/+images/+color/+internal/ycbcr2rgbwideImpl.m`.
//
// Pipeline (per pixel):
//   1. Constants (bps = 10):
//        yzero        = 64                 (Y reference black)
//        ypeak        = 940                (Y reference white)
//        yrange       = ypeak − yzero      = 876
//        chromazero   = 2^(bps − 1)        = 512
//        chromarange  = 960 − 64           = 896 (Cb / Cr full nominal)
//        blackLevel   = 64                 (RGB reference black)
//        nominalPeak  = 940                (RGB reference white)
//        nominalRange = nominalPeak − blackLevel = 876
//      (12-bit equivalents are × 4 except chromarange = 3840 − 256.)
//   2. Y_n  = (Y  − yzero)/yrange;
//      Cb_n = (Cb − chromazero)/chromarange;
//      Cr_n = (Cr − chromazero)/chromarange;
//   3. R = 1.4746·Cr_n + Y_n;
//      B = 1.8814·Cb_n + Y_n;
//      G = (Y_n − 0.2627·R − 0.0593·B) / 0.6780.
//   4. RGB_out = uint16(rgb · nominalRange + blackLevel) with normal
//      uint16 saturation on out-of-range YCbCr inputs.
//
// MATLAB's uint16(...) is round-half-to-nearest-even with saturation
// at [0, 65535]; we mirror that with `+ 0.5` floor (round-half-up).
// Bit-equal on every probed test vector (round-trip with
// rgbwide2ycbcr).
Value ycbcr2rgbwide(const Value &YCBCR, int bits_per_sample,
                    std::pmr::memory_resource *mr)
{
    if (bits_per_sample != 10 && bits_per_sample != 12)
        throw Error("ycbcr2rgbwide: BPS must be 10 or 12",
                    0, 0, "ycbcr2rgbwide", "", "numkit:ycbcr2rgbwide:bps");
    if (YCBCR.type() != ValueType::UINT16)
        throw Error("ycbcr2rgbwide: YCBCR must be UINT16",
                    0, 0, "ycbcr2rgbwide", "", "numkit:ycbcr2rgbwide:class");

    const auto &d = YCBCR.dims();
    const bool is_image = d.is3D();
    if (is_image) {
        if (d.pages() != 3)
            throw Error("ycbcr2rgbwide: H×W×3 image expected",
                        0, 0, "ycbcr2rgbwide", "", "numkit:ycbcr2rgbwide:shape");
    } else {
        if (d.cols() != 3)
            throw Error("ycbcr2rgbwide: p×3 colour list expected",
                        0, 0, "ycbcr2rgbwide", "", "numkit:ycbcr2rgbwide:shape");
    }

    double yzero, yrange, chromazero, chromarange;
    double blackLevel, nominalRange;
    if (bits_per_sample == 10) {
        yzero = 64.0;          yrange      = 876.0;       // 940 - 64
        chromazero = 512.0;    chromarange = 896.0;       // 960 - 64
        blackLevel = 64.0;     nominalRange = 876.0;      // 940 - 64
    } else {
        yzero = 256.0;         yrange      = 3504.0;      // 3760 - 256
        chromazero = 2048.0;   chromarange = 3584.0;      // 3840 - 256
        blackLevel = 256.0;    nominalRange = 3504.0;     // 3760 - 256
    }

    const std::size_t H = d.rows();
    const std::size_t W = is_image ? d.cols() : 1;
    const std::size_t N = H * W;

    Value out;
    if (is_image) out = Value::matrix3d(H, W, 3, ValueType::UINT16, mr);
    else          out = Value::matrix(H, 3, ValueType::UINT16, mr);

    const uint16_t *src = YCBCR.uint16Data();
    uint16_t *dst = out.uint16DataMut();

    const std::size_t ch_stride = is_image ? N : H;

    auto clamp_round = [](double v) -> uint16_t {
        if (v < 0.0)        return 0;
        if (v > 65535.0)    return 65535;
        return static_cast<uint16_t>(v + 0.5);
    };

    for (std::size_t k = 0; k < (is_image ? N : H); ++k) {
        const double Yn  = (static_cast<double>(src[0 * ch_stride + k]) - yzero)
                            / yrange;
        const double Cbn = (static_cast<double>(src[1 * ch_stride + k]) - chromazero)
                            / chromarange;
        const double Crn = (static_cast<double>(src[2 * ch_stride + k]) - chromazero)
                            / chromarange;
        const double R = 1.4746 * Crn + Yn;
        const double B = 1.8814 * Cbn + Yn;
        const double G = (Yn - 0.2627 * R - 0.0593 * B) / 0.6780;
        dst[0 * ch_stride + k] = clamp_round(R * nominalRange + blackLevel);
        dst[1 * ch_stride + k] = clamp_round(G * nominalRange + blackLevel);
        dst[2 * ch_stride + k] = clamp_round(B * nominalRange + blackLevel);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// rgbwide2xyz / xyz2rgbwide — BT.2020/BT.2100 RGB ↔ CIE 1931 XYZ
// ════════════════════════════════════════════════════════════════════
//
// Decodes narrow-range wide-gamut RGB (10- or 12-bit uint16) to CIE
// 1931 XYZ tristimulus values, and the inverse.
//
// Algorithm transliterated verbatim from MATLAB R2025b
//   colorspaces/rgbwide2xyz.m + xyz2rgbwide.m,
//   colorspaces/+images/+color/BT2020RGBEncoder.m,
//   colorspaces/+images/+color/BT2100RGBEncoder.m,
//   colorspaces/+images/+color/+internal/bt2020RGBToXYZTransform.m.
//
// Note: MATLAB's BT.2100 "PQ" path implements the BT.2020-style
// power-curve transfer with fixed α=1.099, β=0.018 (NOT the SMPTE
// ST 2084 perceptual quantizer — the naming in MATLAB is
// counter-intuitive). HLG is the genuine Hybrid Log-Gamma curve.
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

namespace {

// BT.2020 R→XYZ matrix (D65), exact values as computed by MATLAB
// R2025b images.color.internal.bt2020RGBToXYZTransform(). The
// standard primaries (xr=0.708/yr=0.292, xg=0.170/yg=0.797,
// xb=0.131/yb=0.046 with D65=[0.95047, 1, 1.08883]) get solved via
// images.color.internal.computeM; using MATLAB's exact last-digit
// values keeps parity bit-exact.
constexpr double kMBT2020_D65[9] = {
    0.637010191411101,    0.144615027396969,    0.168844781191930,
    0.262721717361640,    0.677989275502262,    0.059289007136098,
    4.994515405547190e-17, 0.028072328847647,    1.060757671152350
};

inline void mat3_mv(const double M[9], const double v[3], double o[3])
{
    o[0] = M[0]*v[0] + M[1]*v[1] + M[2]*v[2];
    o[1] = M[3]*v[0] + M[4]*v[1] + M[5]*v[2];
    o[2] = M[6]*v[0] + M[7]*v[1] + M[8]*v[2];
}

inline void mat3_inv9(const double M[9], double Inv[9])
{
    const double a=M[0],b=M[1],c=M[2],d=M[3],e=M[4],f=M[5],g=M[6],h=M[7],i=M[8];
    const double A = e*i - f*h, B = -(d*i - f*g), C = d*h - e*g;
    const double det = a*A + b*B + c*C;
    const double inv = 1.0/det;
    Inv[0]=A*inv; Inv[1]=-(b*i-c*h)*inv; Inv[2]=(b*f-c*e)*inv;
    Inv[3]=B*inv; Inv[4]=(a*i-c*g)*inv; Inv[5]=-(a*f-c*d)*inv;
    Inv[6]=C*inv; Inv[7]=-(a*h-b*g)*inv; Inv[8]=(a*e-b*d)*inv;
}

// BT.2020 / BT.2100 transfer-function constants per bit depth.
struct BTParams { double alpha, beta; int blackLevel, nominalPeak; };
BTParams bt2020_params(int bps)
{
    if (bps == 10) return {1.099, 0.018, 64, 940};
    if (bps == 12) return {1.0993, 0.0181, 256, 3760};
    throw Error("rgbwide2xyz: bits_per_sample must be 10 or 12",
                0, 0, "rgbwide2xyz", "", "numkit:rgbwide2xyz:bps");
}
BTParams bt2100_params(int bps)
{
    // BT.2100 hardcodes alpha=1.099, beta=0.018 regardless of bit depth
    // for the "PQ" path. For HLG, alpha/beta are not used directly.
    if (bps == 10) return {1.099, 0.018, 64, 940};
    if (bps == 12) return {1.099, 0.018, 256, 3760};
    throw Error("rgbwide2xyz: bits_per_sample must be 10 or 12",
                0, 0, "rgbwide2xyz", "", "numkit:rgbwide2xyz:bps");
}

// Inverse transfer (normalized [0,1] non-linear → linear).
inline double bt_rgb2lin(double v, double alpha, double beta)
{
    if (v < 4.5 * beta) return v / 4.5;
    return std::pow((v + alpha - 1.0) / alpha, 1.0 / 0.45);
}
// Forward transfer (linear → non-linear in [0,1]).
inline double bt_lin2rgb(double v, double alpha, double beta)
{
    if (v < beta) return 4.5 * v;
    return alpha * std::pow(v, 0.45) - (alpha - 1.0);
}

// HLG transfer functions per BT.2100.
inline double hlg_rgb2lin(double v)
{
    constexpr double a = 0.17883277;
    constexpr double b = 1.0 - 4.0 * a;
    const double c = 0.5 - a * std::log(4.0 * a);
    if (v <= 0.5) return (v * v) / 3.0;
    return (std::exp((v - c) / a) + b) / 12.0;
}
inline double hlg_lin2rgb(double v)
{
    constexpr double a = 0.17883277;
    constexpr double b = 1.0 - 4.0 * a;
    const double c = 0.5 - a * std::log(4.0 * a);
    if (v <= 1.0 / 12.0) return std::sqrt(3.0 * v);
    return a * std::log(12.0 * v - b) + c;
}

}  // namespace

Value rgbwide2xyz(const Value &RGB, int bits_per_sample,
                  const std::string &color_space,
                  const std::string &linearization,
                  std::pmr::memory_resource *mr)
{
    std::string cs_lo;
    for (char ch : color_space)
        cs_lo += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    std::string tf_lo;
    for (char ch : linearization)
        tf_lo += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool is_bt2020 = (cs_lo == "bt.2020" || cs_lo == "bt2020");
    bool is_bt2100 = (cs_lo == "bt.2100" || cs_lo == "bt2100");
    if (!is_bt2020 && !is_bt2100)
        throw Error("rgbwide2xyz: ColorSpace must be 'BT.2020' or 'BT.2100'",
                    0, 0, "rgbwide2xyz", "", "numkit:rgbwide2xyz:cs");
    bool use_hlg = (is_bt2100 && tf_lo == "hlg");

    BTParams P = is_bt2020 ? bt2020_params(bits_per_sample)
                            : bt2100_params(bits_per_sample);
    const double nominalRange = static_cast<double>(P.nominalPeak - P.blackLevel);

    const auto &d = RGB.dims();
    if (d.is3D() && d.pages() != 3)
        throw Error("rgbwide2xyz: RGB must be Nx3 or HxWx3",
                    0, 0, "rgbwide2xyz", "", "numkit:rgbwide2xyz:shape");
    const std::size_t H = d.rows();
    const std::size_t W = d.cols();
    const bool is_image = d.is3D();
    const std::size_t Np = is_image ? (H * W) : H;

    Value out;
    if (is_image)
        out = Value::matrix3d(H, W, 3, ValueType::DOUBLE, mr);
    else
        out = Value::matrix(H, 3, ValueType::DOUBLE, mr);

    double *od = out.doubleDataMut();

    auto rgb_to_lin = [&](double v) -> double {
        if (is_bt2020 || !use_hlg) {
            return bt_rgb2lin(v, P.alpha, P.beta);
        }
        return hlg_rgb2lin(v);
    };

    for (std::size_t i = 0; i < Np; ++i) {
        // Read raw R, G, B values.
        std::size_t i_r, i_g, i_b;
        if (is_image) {
            i_r = i;
            i_g = H * W + i;
            i_b = 2 * H * W + i;
        } else {
            i_r = i;
            i_g = H + i;
            i_b = 2 * H + i;
        }
        const double rawR = RGB.elemAsDouble(i_r);
        const double rawG = RGB.elemAsDouble(i_g);
        const double rawB = RGB.elemAsDouble(i_b);
        // Normalize.
        const double nR = (rawR - P.blackLevel) / nominalRange;
        const double nG = (rawG - P.blackLevel) / nominalRange;
        const double nB = (rawB - P.blackLevel) / nominalRange;
        // Inverse transfer (linear).
        const double lR = rgb_to_lin(nR);
        const double lG = rgb_to_lin(nG);
        const double lB = rgb_to_lin(nB);
        // RGB → XYZ.
        const double lin[3] = {lR, lG, lB};
        double xyz[3];
        mat3_mv(kMBT2020_D65, lin, xyz);
        // Write to output.
        if (is_image) {
            od[i_r] = xyz[0];
            od[i_g] = xyz[1];
            od[i_b] = xyz[2];
        } else {
            od[i_r] = xyz[0];
            od[i_g] = xyz[1];
            od[i_b] = xyz[2];
        }
    }
    return out;
}

Value xyz2rgbwide(const Value &XYZ, int bits_per_sample,
                  const std::string &color_space,
                  const std::string &linearization,
                  std::pmr::memory_resource *mr)
{
    std::string cs_lo;
    for (char ch : color_space)
        cs_lo += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    std::string tf_lo;
    for (char ch : linearization)
        tf_lo += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool is_bt2020 = (cs_lo == "bt.2020" || cs_lo == "bt2020");
    bool is_bt2100 = (cs_lo == "bt.2100" || cs_lo == "bt2100");
    if (!is_bt2020 && !is_bt2100)
        throw Error("xyz2rgbwide: ColorSpace must be 'BT.2020' or 'BT.2100'",
                    0, 0, "xyz2rgbwide", "", "numkit:xyz2rgbwide:cs");
    bool use_hlg = (is_bt2100 && tf_lo == "hlg");

    BTParams P = is_bt2020 ? bt2020_params(bits_per_sample)
                            : bt2100_params(bits_per_sample);
    const double nominalRange = static_cast<double>(P.nominalPeak - P.blackLevel);

    double Minv[9];
    mat3_inv9(kMBT2020_D65, Minv);

    const auto &d = XYZ.dims();
    if (d.is3D() && d.pages() != 3)
        throw Error("xyz2rgbwide: XYZ must be Nx3 or HxWx3",
                    0, 0, "xyz2rgbwide", "", "numkit:xyz2rgbwide:shape");
    const std::size_t H = d.rows();
    const std::size_t W = d.cols();
    const bool is_image = d.is3D();
    const std::size_t Np = is_image ? (H * W) : H;

    Value out;
    if (is_image)
        out = Value::matrix3d(H, W, 3, ValueType::UINT16, mr);
    else
        out = Value::matrix(H, 3, ValueType::UINT16, mr);

    uint16_t *od = out.uint16DataMut();
    auto lin_to_rgb = [&](double v) -> double {
        if (is_bt2020 || !use_hlg) {
            return bt_lin2rgb(v < 0.0 ? 0.0 : v, P.alpha, P.beta);
        }
        return hlg_lin2rgb(v < 0.0 ? 0.0 : v);
    };
    auto clamp_to_uint16 = [](double v) -> uint16_t {
        const double r = std::round(v);
        if (r < 0.0)     return 0;
        if (r > 65535.0) return 65535;
        return static_cast<uint16_t>(r);
    };

    for (std::size_t i = 0; i < Np; ++i) {
        std::size_t i_r, i_g, i_b;
        if (is_image) {
            i_r = i;
            i_g = H * W + i;
            i_b = 2 * H * W + i;
        } else {
            i_r = i;
            i_g = H + i;
            i_b = 2 * H + i;
        }
        const double xyz[3] = {
            XYZ.elemAsDouble(i_r),
            XYZ.elemAsDouble(i_g),
            XYZ.elemAsDouble(i_b)
        };
        double lin[3];
        mat3_mv(Minv, xyz, lin);
        // Forward transfer.
        const double nR = lin_to_rgb(lin[0]);
        const double nG = lin_to_rgb(lin[1]);
        const double nB = lin_to_rgb(lin[2]);
        // Un-normalize + clamp.
        od[i_r] = clamp_to_uint16(nR * nominalRange + P.blackLevel);
        od[i_g] = clamp_to_uint16(nG * nominalRange + P.blackLevel);
        od[i_b] = clamp_to_uint16(nB * nominalRange + P.blackLevel);
    }
    return out;
}

namespace detail {

void rgbwide2ycbcr_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgbwide2ycbcr: requires (RGB, BPS)",
                    0, 0, "rgbwide2ycbcr", "", "numkit:rgbwide2ycbcr:nargin");
    const int bps = static_cast<int>(args[1].toScalar());
    outs[0] = rgbwide2ycbcr(args[0], bps, ctx.engine->resource());
}

void ycbcr2rgbwide_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ycbcr2rgbwide: requires (YCBCR, BPS)",
                    0, 0, "ycbcr2rgbwide", "", "numkit:ycbcr2rgbwide:nargin");
    const int bps = static_cast<int>(args[1].toScalar());
    outs[0] = ycbcr2rgbwide(args[0], bps, ctx.engine->resource());
}

void rgbwide2xyz_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rgbwide2xyz: requires (RGB, BPS [, NV...])",
                    0, 0, "rgbwide2xyz", "",
                    "numkit:rgbwide2xyz:nargin");
    auto *mr = ctx.engine->resource();
    const int bps = static_cast<int>(args[1].toScalar());
    std::string cs = "BT.2020";
    std::string lin = "PQ";
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("rgbwide2xyz: expected NV-pair name string",
                        0, 0, "rgbwide2xyz", "",
                        "numkit:rgbwide2xyz:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name) nlo += static_cast<char>(std::tolower(
            static_cast<unsigned char>(ch)));
        if (nlo == "colorspace") cs = args[i + 1].toString();
        else if (nlo == "linearizationfcn") lin = args[i + 1].toString();
        else if (nlo == "whitepoint") {
            // Accepted-but-ignored — only D65 path is implemented this cycle.
        } else {
            throw Error("rgbwide2xyz: unknown option '" + name + "'",
                        0, 0, "rgbwide2xyz", "",
                        "numkit:rgbwide2xyz:unknownNv");
        }
        i += 2;
    }
    outs[0] = rgbwide2xyz(args[0], bps, cs, lin, mr);
}

void xyz2rgbwide_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("xyz2rgbwide: requires (XYZ, BPS [, NV...])",
                    0, 0, "xyz2rgbwide", "",
                    "numkit:xyz2rgbwide:nargin");
    auto *mr = ctx.engine->resource();
    const int bps = static_cast<int>(args[1].toScalar());
    std::string cs = "BT.2020";
    std::string lin = "PQ";
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("xyz2rgbwide: expected NV-pair name string",
                        0, 0, "xyz2rgbwide", "",
                        "numkit:xyz2rgbwide:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name) nlo += static_cast<char>(std::tolower(
            static_cast<unsigned char>(ch)));
        if (nlo == "colorspace") cs = args[i + 1].toString();
        else if (nlo == "linearizationfcn") lin = args[i + 1].toString();
        else if (nlo == "whitepoint") {
            // Accepted-but-ignored — only D65 path is implemented this cycle.
        } else {
            throw Error("xyz2rgbwide: unknown option '" + name + "'",
                        0, 0, "xyz2rgbwide", "",
                        "numkit:xyz2rgbwide:unknownNv");
        }
        i += 2;
    }
    outs[0] = xyz2rgbwide(args[0], bps, cs, lin, mr);
}

void cmunique_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cmunique: requires (X, MAP), (RGB), or (I)",
                    0, 0, "cmunique", "", "numkit:cmunique:nargin");
    auto *mr = ctx.engine->resource();
    std::pair<Value, Value> result;
    if (args.size() == 1) {
        const auto &d = args[0].dims();
        if (d.is3D() && d.pages() == 3)
            result = cmunique_rgb(args[0], mr);
        else
            result = cmunique_i(args[0], mr);
    } else {
        result = cmunique_xm(args[0], args[1], mr);
    }
    outs[0] = std::move(result.first);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(result.second);
}

} // namespace detail (cmunique adapter)

// ── imfuse (composite two images for visual comparison) ───────────
//
// MATLAB R2025b imfuse.m algorithm (programmatic, no imref2d form):
//
//   1. Zero-pad A and B to common size = element-wise max(sz_A, sz_B).
//   2. Per `scaling`:
//        "independent" → scale each to [0, 1] before im2uint8;
//        "joint"       → concatenate, scale together, split;
//        "none"        → just cast to uint8 (via im2uint8).
//   3. Per `method`:
//        "falsecolor"  → grayscale-convert, scale, assign per
//                        ColorChannels to RGB channels.
//        "blend"       → 0.5*A + 0.5*B in overlap (whole image
//                        without imref2d).
//        "diff"        → scale(|A-B|).
//        "checkerboard"→ 8x8 [1 0; 0 1] repmat → imresize-nearest
//                        to image size; A on 1, B on 0.
//        "montage"     → horizontal concat [A B].
namespace {

// Get H, W (ignoring channels) of a 2-D or 3-D image.
inline void hw_of(const Value &I, std::size_t &H, std::size_t &W) {
    H = I.dims().rows();
    W = I.dims().cols();
}

// Channels: 1 for HxW, 3 (or other) for HxWx3.
inline std::size_t channels_of(const Value &I) {
    return I.dims().is3D() ? I.dims().pages() : 1;
}

// Zero-pad I to (outH, outW) with the same channel count.
Value pad_zeros(const Value &I, std::size_t outH, std::size_t outW,
                std::pmr::memory_resource *mr)
{
    std::size_t H, W;
    hw_of(I, H, W);
    const std::size_t C = channels_of(I);
    if (H == outH && W == outW) return I;
    const ValueType T = I.type();
    Value out = (C == 1)
        ? Value::matrix(outH, outW, T, mr)
        : Value::matrix3d(outH, outW, C, T, mr);
    for (std::size_t ch = 0; ch < C; ++ch) {
        for (std::size_t c = 0; c < W; ++c) {
            for (std::size_t r = 0; r < H; ++r) {
                const std::size_t src_idx = (C == 1)
                    ? c * H + r
                    : ch * H * W + c * H + r;
                const std::size_t dst_idx = (C == 1)
                    ? c * outH + r
                    : ch * outH * outW + c * outH + r;
                const double v = I.elemAsDouble(src_idx);
                switch (T) {
                    case ValueType::DOUBLE:  out.doubleDataMut()[dst_idx]  = v; break;
                    case ValueType::SINGLE:  out.singleDataMut()[dst_idx]  = static_cast<float>(v); break;
                    case ValueType::UINT8:   out.uint8DataMut()[dst_idx]   = static_cast<std::uint8_t>(v); break;
                    case ValueType::UINT16:  out.uint16DataMut()[dst_idx]  = static_cast<std::uint16_t>(v); break;
                    case ValueType::UINT32:  out.uint32DataMut()[dst_idx]  = static_cast<std::uint32_t>(v); break;
                    case ValueType::INT8:    out.int8DataMut()[dst_idx]    = static_cast<std::int8_t>(v); break;
                    case ValueType::INT16:   out.int16DataMut()[dst_idx]   = static_cast<std::int16_t>(v); break;
                    case ValueType::INT32:   out.int32DataMut()[dst_idx]   = static_cast<std::int32_t>(v); break;
                    case ValueType::LOGICAL: out.logicalDataMut()[dst_idx] = v != 0.0 ? 1 : 0; break;
                    default: out.doubleDataMut()[dst_idx] = v; break;
                }
            }
        }
    }
    return out;
}

// MATLAB R2025b scaleGrayscaleImage: cast to single, scale [min,max] → [0,1].
Value scale_grayscale_image(const Value &I, std::pmr::memory_resource *mr) {
    if (I.isLogical()) return I;
    const std::size_t N = I.numel();
    Value out = (I.dims().is3D())
        ? Value::matrix3d(I.dims().rows(), I.dims().cols(),
                          I.dims().pages(), ValueType::SINGLE, mr)
        : Value::matrix(I.dims().rows(), I.dims().cols(),
                        ValueType::SINGLE, mr);
    float *od = out.singleDataMut();
    if (N == 0) return out;
    float mn = std::numeric_limits<float>::infinity();
    float mx = -std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < N; ++i) {
        const float v = static_cast<float>(I.elemAsDouble(i));
        od[i] = v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    if (mn == mx) return out;  // constant passthrough as-is
    const float inv = 1.0f / (mx - mn);
    for (std::size_t i = 0; i < N; ++i) od[i] = (od[i] - mn) * inv;
    return out;
}

// Joint scale.
void scale_two_grayscale_images(Value &A, Value &B,
                                std::pmr::memory_resource *mr) {
    float mn = std::numeric_limits<float>::infinity();
    float mx = -std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < A.numel(); ++i) {
        const float v = static_cast<float>(A.elemAsDouble(i));
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    for (std::size_t i = 0; i < B.numel(); ++i) {
        const float v = static_cast<float>(B.elemAsDouble(i));
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    auto rescale = [&](const Value &X) {
        Value Y = X.dims().is3D()
            ? Value::matrix3d(X.dims().rows(), X.dims().cols(),
                              X.dims().pages(), ValueType::SINGLE, mr)
            : Value::matrix(X.dims().rows(), X.dims().cols(),
                            ValueType::SINGLE, mr);
        float *yd = Y.singleDataMut();
        for (std::size_t i = 0; i < X.numel(); ++i) {
            const float v = static_cast<float>(X.elemAsDouble(i));
            yd[i] = (mn == mx) ? v : (v - mn) / (mx - mn);
        }
        return Y;
    };
    A = rescale(A);
    B = rescale(B);
}

void make_similar(Value &A, Value &B, const std::string &scaling,
                  std::pmr::memory_resource *mr)
{
    const bool aIsRGB = A.dims().is3D();
    const bool bIsRGB = B.dims().is3D();
    if (!aIsRGB && !bIsRGB) {
        if (scaling == "joint") {
            scale_two_grayscale_images(A, B, mr);
        } else if (scaling == "independent") {
            A = scale_grayscale_image(A, mr);
            B = scale_grayscale_image(B, mr);
        }
        A = im2uint8(A, mr);
        B = im2uint8(B, mr);
    } else if (aIsRGB && bIsRGB) {
        A = im2uint8(A, mr);
        B = im2uint8(B, mr);
    } else if (aIsRGB && !bIsRGB) {
        if (scaling != "none") B = scale_grayscale_image(B, mr);
        Value Bg = im2uint8(B, mr);
        const std::size_t H = Bg.dims().rows();
        const std::size_t W = Bg.dims().cols();
        Value Brep = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        for (std::size_t ch = 0; ch < 3; ++ch)
            for (std::size_t i = 0; i < H * W; ++i)
                Brep.uint8DataMut()[ch * H * W + i] = Bg.uint8Data()[i];
        B = Brep;
        A = im2uint8(A, mr);
    } else {
        if (scaling != "none") A = scale_grayscale_image(A, mr);
        Value Ag = im2uint8(A, mr);
        const std::size_t H = Ag.dims().rows();
        const std::size_t W = Ag.dims().cols();
        Value Arep = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        for (std::size_t ch = 0; ch < 3; ++ch)
            for (std::size_t i = 0; i < H * W; ++i)
                Arep.uint8DataMut()[ch * H * W + i] = Ag.uint8Data()[i];
        A = Arep;
        B = im2uint8(B, mr);
    }
}

} // anonymous (imfuse helpers)

Value imfuse(const Value &Ain, const Value &Bin,
             const std::string &method,
             const std::string &scaling,
             const Value &channels,
             std::pmr::memory_resource *mr)
{
    static const std::array<const char *, 5> methods{
        "falsecolor", "blend", "diff", "checkerboard", "montage"};
    bool method_ok = false;
    for (auto m : methods) if (method == m) { method_ok = true; break; }
    if (!method_ok)
        throw Error("imfuse: unknown METHOD '" + method
                  + "' (allowed: falsecolor / blend / diff / "
                    "checkerboard / montage)",
                    0, 0, "imfuse", "", "numkit:imfuse:method");
    if (scaling != "independent" && scaling != "joint" && scaling != "none")
        throw Error("imfuse: unknown SCALING '" + scaling + "'",
                    0, 0, "imfuse", "", "numkit:imfuse:scaling");

    std::size_t Ha, Wa, Hb, Wb;
    hw_of(Ain, Ha, Wa);
    hw_of(Bin, Hb, Wb);

    Value A, B;
    if (method == "montage") {
        const std::size_t H = std::max(Ha, Hb);
        A = pad_zeros(Ain, H, Wa, mr);
        B = pad_zeros(Bin, H, Wb, mr);
    } else {
        const std::size_t H = std::max(Ha, Hb);
        const std::size_t W = std::max(Wa, Wb);
        A = pad_zeros(Ain, H, W, mr);
        B = pad_zeros(Bin, H, W, mr);
    }

    if (method == "falsecolor") {
        if (A.dims().is3D()) A = rgb2gray(A, mr);
        if (B.dims().is3D()) B = rgb2gray(B, mr);
        if (scaling == "joint")        scale_two_grayscale_images(A, B, mr);
        else if (scaling == "independent") {
            A = scale_grayscale_image(A, mr);
            B = scale_grayscale_image(B, mr);
        }
        A = im2uint8(A, mr);
        B = im2uint8(B, mr);
        std::array<int, 3> ch{2, 1, 2};  // green-magenta default
        if (channels.numel() == 3) {
            for (int k = 0; k < 3; ++k) {
                const double v = channels.elemAsDouble(k);
                if (v != 0.0 && v != 1.0 && v != 2.0)
                    throw Error("imfuse: ColorChannels values must be 0, 1, or 2",
                                0, 0, "imfuse", "", "numkit:imfuse:channels");
                ch[k] = static_cast<int>(v);
            }
        }
        const std::size_t H = A.dims().rows();
        const std::size_t W = A.dims().cols();
        const std::size_t plane = H * W;
        Value R = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        std::uint8_t *rd = R.uint8DataMut();
        for (int p = 0; p < 3; ++p) {
            const std::uint8_t *src = (ch[p] == 1) ? A.uint8Data()
                                    : (ch[p] == 2) ? B.uint8Data()
                                    : nullptr;
            std::uint8_t *dst = rd + p * plane;
            if (src) std::memcpy(dst, src, plane);
        }
        return R;
    }

    if (method == "blend") {
        make_similar(A, B, scaling, mr);
        const std::size_t H = A.dims().rows();
        const std::size_t W = A.dims().cols();
        const std::size_t C = channels_of(A);
        Value R = (C == 1)
            ? Value::matrix(H, W, ValueType::UINT8, mr)
            : Value::matrix3d(H, W, C, ValueType::UINT8, mr);
        const std::size_t N = H * W * C;
        const std::uint8_t *ap = A.uint8Data();
        const std::uint8_t *bp = B.uint8Data();
        std::uint8_t *rp = R.uint8DataMut();
        for (std::size_t i = 0; i < N; ++i) {
            const float v = 0.5f * static_cast<float>(ap[i])
                          + 0.5f * static_cast<float>(bp[i]);
            int rv = static_cast<int>(v + 0.5f);
            if (rv < 0) rv = 0; else if (rv > 255) rv = 255;
            rp[i] = static_cast<std::uint8_t>(rv);
        }
        return R;
    }

    if (method == "diff") {
        if (A.dims().is3D()) A = rgb2gray(A, mr);
        if (B.dims().is3D()) B = rgb2gray(B, mr);
        if (scaling == "joint") scale_two_grayscale_images(A, B, mr);
        else if (scaling == "independent") {
            A = scale_grayscale_image(A, mr);
            B = scale_grayscale_image(B, mr);
        }
        const std::size_t H = A.dims().rows();
        const std::size_t W = A.dims().cols();
        const std::size_t N = H * W;
        Value diff = Value::matrix(H, W, ValueType::SINGLE, mr);
        for (std::size_t i = 0; i < N; ++i) {
            const float va = static_cast<float>(A.elemAsDouble(i));
            const float vb = static_cast<float>(B.elemAsDouble(i));
            diff.singleDataMut()[i] = std::fabs(va - vb);
        }
        diff = scale_grayscale_image(diff, mr);
        return im2uint8(diff, mr);
    }

    if (method == "checkerboard") {
        make_similar(A, B, scaling, mr);
        const std::size_t H = A.dims().rows();
        const std::size_t W = A.dims().cols();
        const std::size_t C = channels_of(A);
        // 16x16 base = [1 0; 0 1] tiled 8x8.
        Value base = Value::matrix(16, 16, ValueType::LOGICAL, mr);
        std::uint8_t *bbp = base.logicalDataMut();
        for (std::size_t c = 0; c < 16; ++c)
            for (std::size_t r = 0; r < 16; ++r) {
                const std::size_t br = r / 8;
                const std::size_t bc = c / 8;
                bbp[c * 16 + r] = static_cast<std::uint8_t>((br + bc + 1) % 2);
            }
        Value mask = imresize(base, H, W, "nearest", mr);
        Value R = (C == 1)
            ? Value::matrix(H, W, ValueType::UINT8, mr)
            : Value::matrix3d(H, W, C, ValueType::UINT8, mr);
        const std::uint8_t *m = mask.logicalData();
        const std::uint8_t *ap = A.uint8Data();
        const std::uint8_t *bp2 = B.uint8Data();
        std::uint8_t *rp = R.uint8DataMut();
        const std::size_t plane = H * W;
        for (std::size_t ch = 0; ch < C; ++ch) {
            const std::uint8_t *a_ch = ap + ch * plane;
            const std::uint8_t *b_ch = bp2 + ch * plane;
            std::uint8_t *r_ch = rp + ch * plane;
            for (std::size_t i = 0; i < plane; ++i)
                r_ch[i] = m[i] ? a_ch[i] : b_ch[i];
        }
        return R;
    }

    // method == "montage"
    make_similar(A, B, scaling, mr);
    const std::size_t H = A.dims().rows();
    const std::size_t WAa = A.dims().cols();
    const std::size_t WBb = B.dims().cols();
    const std::size_t C = channels_of(A);
    const std::size_t Wt = WAa + WBb;
    Value R = (C == 1)
        ? Value::matrix(H, Wt, ValueType::UINT8, mr)
        : Value::matrix3d(H, Wt, C, ValueType::UINT8, mr);
    const std::uint8_t *ap = A.uint8Data();
    const std::uint8_t *bp = B.uint8Data();
    std::uint8_t *rp = R.uint8DataMut();
    for (std::size_t ch = 0; ch < C; ++ch) {
        for (std::size_t c = 0; c < WAa; ++c)
            for (std::size_t r = 0; r < H; ++r)
                rp[ch * H * Wt + c * H + r]
                    = ap[ch * H * WAa + c * H + r];
        for (std::size_t c = 0; c < WBb; ++c)
            for (std::size_t r = 0; r < H; ++r)
                rp[ch * H * Wt + (WAa + c) * H + r]
                    = bp[ch * H * WBb + c * H + r];
    }
    return R;
}

// ── tonemap (HDR → LDR for display) ─────────────────────────────
//
// MATLAB R2025b tonemap.m algorithm:
//   1. min_nz = min over non-zero entries of HDR.
//   2. Replace zeros with min_nz.
//   3. log2 → mat2gray (global min/max → [0, 1]).
//   4. Grayscale: adapthisteq(NumTiles) → imadjust(LRemap, [0 1]).
//      RGB: rgb2lab → L/100 → adapthisteq → imadjust → *100;
//           a,b channels × saturation; lab2rgb.
//   5. im2uint8.
//
// References:
//   G. Ward et al., "A Visibility Matching Tone Reproduction
//   Operator for High Dynamic Range Scenes", IEEE TVCG 3(4), 1997.
Value tonemap(const Value &HDR,
              double lremap_lo, double lremap_hi,
              double saturation,
              int ntilesR, int ntilesC,
              std::pmr::memory_resource *mr)
{
    if (!(lremap_lo >= 0.0 && lremap_lo <= 1.0
       && lremap_hi >= 0.0 && lremap_hi <= 1.0
       && lremap_lo < lremap_hi))
        throw Error("tonemap: AdjustLightness must satisfy "
                    "0 <= lo < hi <= 1",
                    0, 0, "tonemap", "", "numkit:tonemap:adjust");
    if (!(saturation >= 0))
        throw Error("tonemap: AdjustSaturation must be non-negative",
                    0, 0, "tonemap", "", "numkit:tonemap:sat");
    if (ntilesR < 2 || ntilesC < 2)
        throw Error("tonemap: NumberOfTiles values must be >= 2",
                    0, 0, "tonemap", "", "numkit:tonemap:tiles");

    const std::size_t H = HDR.dims().rows();
    const std::size_t W = HDR.dims().cols();
    const bool isRGB = HDR.dims().is3D();
    if (isRGB && HDR.dims().pages() != 3)
        throw Error("tonemap: HDR must be H×W or H×W×3",
                    0, 0, "tonemap", "", "numkit:tonemap:shape");
    const std::size_t C = isRGB ? 3 : 1;
    const std::size_t plane = H * W;
    const std::size_t Ntot = plane * C;

    // Step 1+2: read into double; find min nonzero; replace zeros with min_nz.
    std::pmr::vector<double> data(Ntot, 0.0, mr);
    double min_nz = std::numeric_limits<double>::infinity();
    bool any_nz = false;
    for (std::size_t i = 0; i < Ntot; ++i) {
        const double v = HDR.elemAsDouble(i);
        if (v < 0.0)
            throw Error("tonemap: HDR must be non-negative",
                        0, 0, "tonemap", "", "numkit:tonemap:negative");
        data[i] = v;
        if (v != 0.0) {
            any_nz = true;
            if (v < min_nz) min_nz = v;
        }
    }

    if (!any_nz) {
        // All zeros → output is all zeros uint8.
        Value out = isRGB
            ? Value::matrix3d(H, W, 3, ValueType::UINT8, mr)
            : Value::matrix(H, W, ValueType::UINT8, mr);
        std::memset(out.uint8DataMut(), 0, Ntot);
        return out;
    }
    for (std::size_t i = 0; i < Ntot; ++i)
        if (data[i] == 0.0) data[i] = min_nz;

    // Step 3: log2, then mat2gray to [0, 1].
    double gmin = std::numeric_limits<double>::infinity();
    double gmax = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < Ntot; ++i) {
        const double v = std::log2(data[i]);
        data[i] = v;
        if (v < gmin) gmin = v;
        if (v > gmax) gmax = v;
    }
    const double span = gmax - gmin;
    if (span > 0.0) {
        const double inv = 1.0 / span;
        for (std::size_t i = 0; i < Ntot; ++i) data[i] = (data[i] - gmin) * inv;
    } else {
        // Constant log image → all zeros after mat2gray.
        std::fill(data.begin(), data.end(), 0.0);
    }

    // Wrap into a Value for downstream helpers.
    auto wrap_plane = [&](std::size_t ch) {
        Value V = Value::matrix(H, W, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < plane; ++i)
            V.doubleDataMut()[i] = data[ch * plane + i];
        return V;
    };

    AdaptHistEqOptions opts;
    opts.numTilesR = ntilesR;
    opts.numTilesC = ntilesC;

    if (!isRGB) {
        // Grayscale: adapthisteq → imadjust.
        Value G = wrap_plane(0);
        G = adapthisteq(G, opts, mr);
        G = imadjust(G, lremap_lo, lremap_hi, 0.0, 1.0, 1.0, mr);
        return im2uint8(G, mr);
    }

    // RGB path.
    Value RGB = Value::matrix3d(H, W, 3, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < Ntot; ++i)
        RGB.doubleDataMut()[i] = data[i];
    Value Lab = rgb2lab(RGB, mr);  // H x W x 3 double

    // L / 100.
    for (std::size_t i = 0; i < plane; ++i)
        Lab.doubleDataMut()[i] /= 100.0;
    // Wrap L into a 2-D image; apply adapthisteq + imadjust; write back.
    Value Lplane = Value::matrix(H, W, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < plane; ++i)
        Lplane.doubleDataMut()[i] = Lab.doubleData()[i];
    Lplane = adapthisteq(Lplane, opts, mr);
    Lplane = imadjust(Lplane, lremap_lo, lremap_hi, 0.0, 1.0, 1.0, mr);
    for (std::size_t i = 0; i < plane; ++i)
        Lab.doubleDataMut()[i] = Lplane.elemAsDouble(i) * 100.0;
    // a, b * saturation.
    for (std::size_t i = 0; i < plane; ++i) {
        Lab.doubleDataMut()[plane + i]      *= saturation;
        Lab.doubleDataMut()[2 * plane + i] *= saturation;
    }
    Value rgb_out = lab2rgb(Lab, mr);  // H x W x 3 double in [0, 1] (clipped)
    return im2uint8(rgb_out, mr);
}

namespace detail {

void imfuse_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imfuse: requires (A, B [, METHOD] [, NV...])",
                    0, 0, "imfuse", "", "numkit:imfuse:nargin");
    auto *mr = ctx.engine->resource();

    const Value &A = args[0];
    const Value &B = args[1];

    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::string method = "falsecolor";
    std::string scaling = "independent";
    Value channels;  // empty → default (green-magenta)
    std::size_t i = 2;
    if (i < args.size() && is_string(args[i])) {
        std::string m = args[i].toString();
        static const std::array<const char *, 5> mset{
            "falsecolor", "blend", "diff", "checkerboard", "montage"};
        static const std::array<const char *, 2> nvset{
            "Scaling", "ColorChannels"};
        bool is_method = false;
        for (auto mm : mset) if (m == mm) { is_method = true; break; }
        bool is_nv = false;
        // Case-insensitive NV-name check.
        std::string mlo;
        for (char ch : m)
            mlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (mlo == "scaling" || mlo == "colorchannels") is_nv = true;
        if (is_method) { method = m; ++i; }
        else if (!is_nv)
            throw Error("imfuse: unknown METHOD '" + m + "' (allowed: "
                        "falsecolor / blend / diff / checkerboard / "
                        "montage)",
                        0, 0, "imfuse", "", "numkit:imfuse:method");
        // else: leave i=2 so it gets parsed as NV pair below.
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imfuse: expected NV-pair name string",
                        0, 0, "imfuse", "", "numkit:imfuse:badNvArg");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "scaling") {
            scaling = args[i + 1].toString();
            // MATLAB lowercases for switch — replicate.
            std::string slo;
            for (char ch : scaling)
                slo += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            scaling = slo;
        } else if (nlo == "colorchannels") {
            const Value &v = args[i + 1];
            if (is_string(v)) {
                std::string s = v.toString();
                std::string slo;
                for (char ch : s)
                    slo += static_cast<char>(std::tolower(
                        static_cast<unsigned char>(ch)));
                channels = Value::matrix(1, 3, ValueType::DOUBLE, mr);
                if (slo == "red-cyan") {
                    channels.doubleDataMut()[0] = 1;
                    channels.doubleDataMut()[1] = 2;
                    channels.doubleDataMut()[2] = 2;
                } else if (slo == "green-magenta") {
                    channels.doubleDataMut()[0] = 2;
                    channels.doubleDataMut()[1] = 1;
                    channels.doubleDataMut()[2] = 2;
                } else {
                    throw Error("imfuse: unknown ColorChannels '" + s + "'",
                                0, 0, "imfuse", "", "numkit:imfuse:channels");
                }
            } else {
                channels = v;
            }
        } else {
            throw Error("imfuse: unknown option '" + name + "'",
                        0, 0, "imfuse", "", "numkit:imfuse:unknownNv");
        }
        i += 2;
    }
    outs[0] = imfuse(A, B, method, scaling, channels, mr);
}

void tonemap_reg(Span<const Value> args, std::size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tonemap: requires (HDR [, NV...])",
                    0, 0, "tonemap", "", "numkit:tonemap:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    double lo = 0.0, hi = 1.0;
    double saturation = 1.0;
    int ntilesR = 4, ntilesC = 4;

    std::size_t i = 1;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("tonemap: expected NV-pair name",
                        0, 0, "tonemap", "", "numkit:tonemap:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "adjustlightness") {
            const Value &v = args[i + 1];
            if (v.numel() != 2)
                throw Error("tonemap: AdjustLightness must be [lo hi]",
                            0, 0, "tonemap", "", "numkit:tonemap:adjustLen");
            lo = v.elemAsDouble(0);
            hi = v.elemAsDouble(1);
        } else if (nlo == "adjustsaturation") {
            saturation = args[i + 1].toScalar();
        } else if (nlo == "numberoftiles") {
            const Value &v = args[i + 1];
            if (v.numel() == 1) {
                ntilesR = ntilesC = static_cast<int>(v.toScalar());
            } else if (v.numel() == 2) {
                ntilesR = static_cast<int>(v.elemAsDouble(0));
                ntilesC = static_cast<int>(v.elemAsDouble(1));
            } else {
                throw Error("tonemap: NumberOfTiles must be a scalar or "
                            "2-element vector",
                            0, 0, "tonemap", "", "numkit:tonemap:tilesLen");
            }
        } else {
            throw Error("tonemap: unknown option '" + name + "'",
                        0, 0, "tonemap", "", "numkit:tonemap:unknownNv");
        }
        i += 2;
    }
    outs[0] = tonemap(args[0], lo, hi, saturation, ntilesR, ntilesC, mr);
}

} // namespace detail

// ════════════════════════════════════════════════════════════════════
// labeloverlay — colour-overlay a label / mask matrix on a 2-D image
// ════════════════════════════════════════════════════════════════════
//
// Algorithm transliterated verbatim from MATLAB R2025b
//   toolbox/images/images/labeloverlay.m
//   toolbox/images/images/+images/+internal/LabelColormapHelper.m
//   toolbox/images/images/+images/+internal/labeloverlayalgo.m
//   toolbox/images/images/+images/+internal/greedyGraphColoring.m
//
// Pipeline:
//   1. im2single(A) → A in [0,1]; replicate grayscale to RGB.
//   2. maxLabel = max(L(:)), totalLabels = maxLabel + 1.
//   3. Resolve cmap:
//        - named string → feval(name, totalLabels)
//        - numeric Nx3 → as-is
//   4. ColorAssignment:
//        - auto: noshuffle if numeric, shuffle if string
//        - shuffle: cmap = cmap(randperm(N), :)
//                   randperm uses MATLAB MT19937 seed-0 (rng('default')).
//        - contrasting-neighbors: greedy BFS graph colouring on 8-conn.
//   5. alphaVal = 1 - transparency. Build alphamap of length N:
//        - if 0 ∈ included: alphamap(included+1) = alphaVal
//        - else: alphamap(included) = alphaVal; cmap = [cmap(1,:);cmap];
//                alphamap = [0, alphamap].
//   6. Per pixel: B(r,c,ch) = (1-α[L+1]) · A(r,c,ch) + α[L+1] · cmap[L+1,ch]
//   7. im2uint8(B) → uint8 H×W×3.
//
// Bit-identical with MATLAB on `noshuffle` and on `shuffle` for all
// probed seeds (default = 0). `contrasting-neighbors` is bit-identical
// where MATLAB's greedy BFS visits nodes in the same order (the
// canonical column-major BFS does on integer-typed L matrices).

namespace {

// ── jet(N) — MATLAB-canonical jet colormap ────────────────────────
// Reproduces MATLAB R2025b graphics/jet.m exactly for N >= 1.
//
//   n = ceil(m/4);
//   u = [(1:n)/n; ones(n-1,1); (n:-1:1)/n];
//   g = ceil(n/2) - (mod(m,4)==1) + (1:length(u))';
//   r = g + n;   b = g - n;
//   g(g>m)=[]; r(r>m)=[]; b(b<1)=[];
//   J(:,:) = 0;
//   J(r,1) = u(1:length(r));
//   J(g,2) = u(1:length(g));
//   J(b,3) = u(end-length(b)+1:end);
Value jet_colormap(int m, std::pmr::memory_resource *mr)
{
    if (m < 1) m = 1;
    Value out = Value::matrix(static_cast<std::size_t>(m), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::fill(od, od + 3 * static_cast<std::size_t>(m), 0.0);
    const int n = (m + 3) / 4;            // ceil(m/4)
    const int ulen = n + (n - 1) + n;      // length(u)
    auto u_at = [&](int i) -> double {     // 1-based
        if (i <= n) return double(i) / double(n);
        if (i <= 2 * n - 1) return 1.0;
        return double(2 * n - 1 + n - (i - 1)) / double(n);  // (n:-1:1)/n
    };
    const int gbase = (n + 1) / 2 - (m % 4 == 1 ? 1 : 0);   // 1-based base
    // For each k=1..ulen: g_k = gbase + k, r_k = g_k + n, b_k = g_k - n.
    int rcount = 0, gcount = 0, bcount = 0;
    for (int k = 1; k <= ulen; ++k) {
        const int g = gbase + k;
        const int r = g + n;
        const int b = g - n;
        if (r >= 1 && r <= m) {
            // u(1..length(r)) — k-th valid r maps to u(rcount+1)
            ++rcount;
            od[0 * static_cast<std::size_t>(m) + (r - 1)] = u_at(rcount);
        }
        if (g >= 1 && g <= m) {
            ++gcount;
            od[1 * static_cast<std::size_t>(m) + (g - 1)] = u_at(gcount);
        }
        if (b >= 1 && b <= m) ++bcount;  // tally only; assigned below
    }
    // b uses u(end-length(b)+1 : end) — i.e. the last bcount entries.
    int bsofar = 0;
    for (int k = 1; k <= ulen; ++k) {
        const int g = gbase + k;
        const int b = g - n;
        if (b >= 1 && b <= m) {
            ++bsofar;
            od[2 * static_cast<std::size_t>(m) + (b - 1)] =
                u_at(ulen - bcount + bsofar);
        }
    }
    return out;
}

// ── hsv(N) — MATLAB-canonical HSV colormap ────────────────────────
// hsv2rgb([h s v]) with h = (0:N-1)'/N, s=v=1.
Value hsv_colormap(int m, std::pmr::memory_resource *mr)
{
    if (m < 1) m = 1;
    Value out = Value::matrix(static_cast<std::size_t>(m), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (int i = 0; i < m; ++i) {
        const double h = double(i) / double(m);     // [0,1)
        // HSV→RGB, with s=v=1.
        const double hh = h * 6.0;
        const int sect = static_cast<int>(std::floor(hh));
        const double f = hh - sect;
        double r = 0, g = 0, b = 0;
        switch (sect % 6) {
            case 0: r = 1;     g = f;     b = 0;     break;
            case 1: r = 1 - f; g = 1;     b = 0;     break;
            case 2: r = 0;     g = 1;     b = f;     break;
            case 3: r = 0;     g = 1 - f; b = 1;     break;
            case 4: r = f;     g = 0;     b = 1;     break;
            case 5: r = 1;     g = 0;     b = 1 - f; break;
        }
        od[0 * static_cast<std::size_t>(m) + i] = r;
        od[1 * static_cast<std::size_t>(m) + i] = g;
        od[2 * static_cast<std::size_t>(m) + i] = b;
    }
    return out;
}

// ── parula(N) — MATLAB-canonical parula colormap ──────────────────
// 64-row reference table from MATLAB R2025b parula.m, linearly
// interpolated for any N. Reference table copy-pasted verbatim from
// MATLAB output: type(jet) replaced with type(parula).
static const double kParulaRef[64 * 3] = {
    0.2422, 0.1504, 0.6603, 0.2504, 0.1650, 0.7076, 0.2578, 0.1818, 0.7511,
    0.2647, 0.1978, 0.7952, 0.2706, 0.2147, 0.8364, 0.2751, 0.2342, 0.8710,
    0.2783, 0.2559, 0.8991, 0.2803, 0.2782, 0.9221, 0.2813, 0.3006, 0.9414,
    0.2810, 0.3228, 0.9579, 0.2795, 0.3447, 0.9717, 0.2760, 0.3667, 0.9829,
    0.2699, 0.3892, 0.9906, 0.2602, 0.4123, 0.9952, 0.2440, 0.4358, 0.9988,
    0.2206, 0.4603, 0.9973, 0.1963, 0.4847, 0.9892, 0.1834, 0.5074, 0.9798,
    0.1786, 0.5289, 0.9682, 0.1764, 0.5499, 0.9520, 0.1687, 0.5703, 0.9359,
    0.1540, 0.5902, 0.9218, 0.1460, 0.6091, 0.9079, 0.1380, 0.6276, 0.8973,
    0.1248, 0.6459, 0.8883, 0.1113, 0.6635, 0.8763, 0.0952, 0.6798, 0.8598,
    0.0689, 0.6948, 0.8394, 0.0297, 0.7082, 0.8163, 0.0036, 0.7203, 0.7917,
    0.0067, 0.7312, 0.7660, 0.0433, 0.7411, 0.7394, 0.0964, 0.7500, 0.7120,
    0.1408, 0.7584, 0.6842, 0.1717, 0.7670, 0.6554, 0.1938, 0.7758, 0.6251,
    0.2161, 0.7843, 0.5923, 0.2470, 0.7918, 0.5567, 0.2906, 0.7973, 0.5188,
    0.3406, 0.8008, 0.4789, 0.3909, 0.8029, 0.4354, 0.4456, 0.8024, 0.3909,
    0.5044, 0.7993, 0.3480, 0.5616, 0.7942, 0.3045, 0.6174, 0.7876, 0.2612,
    0.6720, 0.7793, 0.2227, 0.7242, 0.7698, 0.1910, 0.7738, 0.7598, 0.1646,
    0.8203, 0.7498, 0.1535, 0.8634, 0.7406, 0.1596, 0.9035, 0.7330, 0.1774,
    0.9393, 0.7288, 0.2100, 0.9728, 0.7298, 0.2394, 0.9956, 0.7434, 0.2371,
    0.9970, 0.7659, 0.2199, 0.9952, 0.7893, 0.2028, 0.9892, 0.8136, 0.1885,
    0.9786, 0.8386, 0.1766, 0.9676, 0.8639, 0.1643, 0.9610, 0.8890, 0.1537,
    0.9597, 0.9135, 0.1423, 0.9628, 0.9373, 0.1265, 0.9691, 0.9606, 0.1064,
    0.9769, 0.9839, 0.0805
};

Value parula_colormap(int m, std::pmr::memory_resource *mr)
{
    if (m < 1) m = 1;
    Value out = Value::matrix(static_cast<std::size_t>(m), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (m == 1) {
        od[0] = kParulaRef[0];
        od[1] = kParulaRef[1];
        od[2] = kParulaRef[2];
        return out;
    }
    // Linear interp from 64-row reference. MATLAB uses interp1 with
    // 'linear' between sample positions (0:63)/63 → (0:m-1)/(m-1).
    for (int i = 0; i < m; ++i) {
        const double t = double(i) / double(m - 1);
        const double src = t * 63.0;
        int lo = static_cast<int>(std::floor(src));
        int hi = lo + 1;
        if (hi > 63) { hi = 63; lo = 62; }
        if (lo < 0)  { lo = 0;  hi = 1;  }
        const double a = src - lo;
        for (int ch = 0; ch < 3; ++ch) {
            const double v = (1 - a) * kParulaRef[lo * 3 + ch]
                           + a       * kParulaRef[hi * 3 + ch];
            od[ch * static_cast<std::size_t>(m) + i] = v;
        }
    }
    return out;
}

// ── Other named colormaps — minimal set needed for labeloverlay ───
// gray, hot, cool, bone — all are simple closed-form linear ramps.
Value gray_colormap(int m, std::pmr::memory_resource *mr)
{
    if (m < 1) m = 1;
    Value out = Value::matrix(static_cast<std::size_t>(m), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (int i = 0; i < m; ++i) {
        const double g = (m == 1) ? 0.0 : double(i) / double(m - 1);
        od[0 * m + i] = g;
        od[1 * m + i] = g;
        od[2 * m + i] = g;
    }
    return out;
}

Value hot_colormap(int m, std::pmr::memory_resource *mr)
{
    // From MATLAB R2025b graphics/hot.m:
    //   n = fix(3/8*m);  r = [(1:n)'/n; ones(m-n,1)];
    //   g = [zeros(n,1); (1:n)'/n; ones(m-2*n,1)];
    //   b = [zeros(2*n,1); (1:m-2*n)'/(m-2*n)];
    if (m < 1) m = 1;
    Value out = Value::matrix(static_cast<std::size_t>(m), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::fill(od, od + 3 * m, 0.0);
    const int n = (3 * m) / 8;
    for (int i = 0; i < m; ++i) {
        double r, g, b;
        if (i < n)               r = double(i + 1) / double(n);
        else                     r = 1.0;
        if (i < n)               g = 0.0;
        else if (i < 2 * n)      g = double(i - n + 1) / double(n);
        else                     g = 1.0;
        if (i < 2 * n)           b = 0.0;
        else                     b = double(i - 2 * n + 1) / double(m - 2 * n);
        od[0 * m + i] = r;
        od[1 * m + i] = g;
        od[2 * m + i] = b;
    }
    return out;
}

Value cool_colormap(int m, std::pmr::memory_resource *mr)
{
    // From graphics/cool.m: r = (0:m-1)'/(m-1); g = 1-r; b = ones(m,1).
    if (m < 1) m = 1;
    Value out = Value::matrix(static_cast<std::size_t>(m), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (int i = 0; i < m; ++i) {
        const double r = (m == 1) ? 0.0 : double(i) / double(m - 1);
        od[0 * m + i] = r;
        od[1 * m + i] = 1.0 - r;
        od[2 * m + i] = 1.0;
    }
    return out;
}

Value bone_colormap(int m, std::pmr::memory_resource *mr)
{
    // bone(m) = (7*gray(m) + hsv-blue ramp)/8 in MATLAB. Specifically:
    //   bone = (7*gray(m) + [zeros(...); (1:n)'/n; ones(...); ones(...)])/8
    // Use the form: bone(m) = (7*gray(m) + flipud(hot(m))(:,[3 2 1]))/8.
    if (m < 1) m = 1;
    Value g = gray_colormap(m, mr);
    Value h = hot_colormap(m, mr);
    Value out = Value::matrix(static_cast<std::size_t>(m), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *gd = g.doubleData();
    const double *hd = h.doubleData();
    for (int i = 0; i < m; ++i) {
        // flipud(hot) with channel swap [3 2 1]:
        const int j = m - 1 - i;
        const double r2 = hd[2 * m + j];  // B of flipped → R
        const double g2 = hd[1 * m + j];
        const double b2 = hd[0 * m + j];  // R of flipped → B
        od[0 * m + i] = (7.0 * gd[0 * m + i] + r2) / 8.0;
        od[1 * m + i] = (7.0 * gd[1 * m + i] + g2) / 8.0;
        od[2 * m + i] = (7.0 * gd[2 * m + i] + b2) / 8.0;
    }
    return out;
}

// Dispatch a named MATLAB colormap. Throws for anything not in the
// supported set.
Value resolve_named_colormap(const std::string &name, int N,
                             std::pmr::memory_resource *mr)
{
    std::string lo;
    for (char ch : name)
        lo += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (lo == "jet")     return jet_colormap(N, mr);
    if (lo == "hsv")     return hsv_colormap(N, mr);
    if (lo == "parula")  return parula_colormap(N, mr);
    if (lo == "gray" || lo == "grey")
                         return gray_colormap(N, mr);
    if (lo == "hot")     return hot_colormap(N, mr);
    if (lo == "cool")    return cool_colormap(N, mr);
    if (lo == "bone")    return bone_colormap(N, mr);
    throw Error("labeloverlay: unsupported colormap name '" + name +
                "' (supported: jet, hsv, parula, gray, hot, cool, bone)",
                0, 0, "labeloverlay", "", "numkit:labeloverlay:cmapName");
}

// ── randperm(N) using MATLAB MT19937 with rng('default') ──────────
// Bit-identical with `rng('default'); randperm(N)` in MATLAB R2025b.
// Strategy: MATLAB's randperm(N) for N == k internally does
// `[~, p] = sort(rand(1, N))`. Using genRes53() draws, the indices
// after a stable sort give the permutation.
std::vector<int> matlab_default_randperm(int N)
{
    ::numkit::builtin::detail::MatlabMT19937 rng;   // seed 0 → state[0] = 5489
    std::vector<double> u(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) u[i] = rng.genRes53();
    std::vector<int> p(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) p[i] = i;          // 0-based for sort
    std::stable_sort(p.begin(), p.end(),
                     [&](int a, int b) { return u[a] < u[b]; });
    for (int &x : p) x += 1;                       // → 1-based result
    return p;
}

// Permute rows of an Nx3 cmap by `perm` (1-based).
Value permute_cmap_rows(const Value &cmap, const std::vector<int> &perm,
                        std::pmr::memory_resource *mr)
{
    const std::size_t N = cmap.dims().rows();
    Value out = Value::matrix(N, 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *cd = cmap.doubleData();
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t src = static_cast<std::size_t>(perm[i] - 1);
        for (int ch = 0; ch < 3; ++ch)
            od[ch * N + i] = cd[ch * N + src];
    }
    return out;
}

// ── Greedy graph colouring (BFS) for contrasting-neighbors ────────
// Builds 8-conn adjacency between distinct non-zero labels, then
// BFS-assigns colours from a "maximally distinct" palette built by
// `select_maximally_distinct`. Bit-identical with MATLAB's
// `images.internal.greedyGraphColoring` when L is integer-typed.
//
// Reference: `images.internal.greedyGraphColoring.m` (R2025b).
struct EdgeSet {
    std::vector<std::pair<int, int>> edges;  // unordered (a < b)
    int maxLabel = 0;
};

EdgeSet build_8conn_edges(const Value &L)
{
    EdgeSet s;
    const std::size_t H = L.dims().rows();
    const std::size_t W = L.dims().cols();
    auto lab = [&](std::size_t r, std::size_t c) -> int {
        return static_cast<int>(L.elemAsDouble(c * H + r));
    };
    // Dedup with set<pair>.
    std::set<std::pair<int, int>> uniq;
    for (std::size_t r = 0; r < H; ++r)
        for (std::size_t c = 0; c < W; ++c) {
            const int a = lab(r, c);
            if (a > s.maxLabel) s.maxLabel = a;
            // 8-conn forward neighbours: (r+1,c), (r,c+1), (r+1,c+1), (r+1,c-1)
            const std::array<std::pair<int, int>, 4> off{{
                {1, 0}, {0, 1}, {1, 1}, {1, -1}
            }};
            for (auto [dr, dc] : off) {
                const std::size_t rr = r + dr;
                const long cc = static_cast<long>(c) + dc;
                if (rr >= H || cc < 0 || static_cast<std::size_t>(cc) >= W)
                    continue;
                const int b = lab(rr, static_cast<std::size_t>(cc));
                if (a == b) continue;
                int x = a, y = b;
                if (x > y) std::swap(x, y);
                uniq.emplace(x, y);
            }
        }
    s.edges.assign(uniq.begin(), uniq.end());
    return s;
}

int chromatic_upper_bound(const EdgeSet &s)
{
    // Brooke's theorem: ub = max degree + 1.
    std::map<int, int> deg;
    for (auto [a, b] : s.edges) { deg[a]++; deg[b]++; }
    int md = 0;
    for (auto [_, d] : deg) if (d > md) md = d;
    return md + 1;
}

// Select up to K maximally distinct colours from cmap (by RGB distance,
// greedy "farthest from previously selected"). MATLAB's algorithm
// (`images.internal.selectMaximallyDistinctColors`) starts with the
// last colour and at each step picks the unselected colour with the
// maximum sum of distances to already-selected. Distance is plain L2
// in RGB. Returns K colours as the first K rows of cmap_out.
Value select_maximally_distinct(const Value &cmap, int K,
                                std::pmr::memory_resource *mr)
{
    const std::size_t N = cmap.dims().rows();
    if (K > static_cast<int>(N)) K = static_cast<int>(N);
    Value out = Value::matrix(K, 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *cd = cmap.doubleData();
    std::vector<char> picked(N, 0);
    std::vector<double> dist_sum(N, 0.0);
    // First pick: last row of cmap (matches MATLAB).
    int last = static_cast<int>(N - 1);
    picked[last] = 1;
    od[0 * K + 0] = cd[0 * N + last];
    od[1 * K + 0] = cd[1 * N + last];
    od[2 * K + 0] = cd[2 * N + last];
    for (std::size_t i = 0; i < N; ++i)
        if (!picked[i]) {
            const double dr = cd[0 * N + i] - cd[0 * N + last];
            const double dg = cd[1 * N + i] - cd[1 * N + last];
            const double db = cd[2 * N + i] - cd[2 * N + last];
            dist_sum[i] = std::sqrt(dr * dr + dg * dg + db * db);
        }
    for (int k = 1; k < K; ++k) {
        // Pick argmax of dist_sum among unpicked.
        int best = -1;
        double bv = -1.0;
        for (std::size_t i = 0; i < N; ++i)
            if (!picked[i] && dist_sum[i] > bv) {
                bv = dist_sum[i];
                best = static_cast<int>(i);
            }
        if (best < 0) break;
        picked[best] = 1;
        od[0 * K + k] = cd[0 * N + best];
        od[1 * K + k] = cd[1 * N + best];
        od[2 * K + k] = cd[2 * N + best];
        // Update dist_sum.
        for (std::size_t i = 0; i < N; ++i)
            if (!picked[i]) {
                const double dr = cd[0 * N + i] - cd[0 * N + best];
                const double dg = cd[1 * N + i] - cd[1 * N + best];
                const double db = cd[2 * N + i] - cd[2 * N + best];
                dist_sum[i] += std::sqrt(dr * dr + dg * dg + db * db);
            }
    }
    return out;
}

Value greedy_graph_coloring(const Value &L_in, const Value &cmap,
                            std::pmr::memory_resource *mr)
{
    // Replicate MATLAB: add zero-color row if 0 present in L.
    bool tf_zero = false;
    const std::size_t H = L_in.dims().rows();
    const std::size_t W = L_in.dims().cols();
    for (std::size_t i = 0; i < H * W; ++i)
        if (L_in.elemAsDouble(i) == 0.0) { tf_zero = true; break; }

    Value L = L_in;
    Value cmap_used = cmap;
    if (tf_zero) {
        // L = L + 1
        Value Lshift = Value::matrix(H, W, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < H * W; ++i)
            Lshift.doubleDataMut()[i] = L_in.elemAsDouble(i) + 1.0;
        L = std::move(Lshift);
        // cmap = [cmap(1,:); cmap]   (use cmap row 1 as the zero
        // colour, since labeloverlay doesn't pass an explicit zero).
        const std::size_t N = cmap.dims().rows();
        Value c2 = Value::matrix(N + 1, 3, ValueType::DOUBLE, mr);
        const double *cd = cmap.doubleData();
        double *cd2 = c2.doubleDataMut();
        for (int ch = 0; ch < 3; ++ch) {
            cd2[ch * (N + 1) + 0] = cd[ch * N + 0];
            for (std::size_t i = 0; i < N; ++i)
                cd2[ch * (N + 1) + (i + 1)] = cd[ch * N + i];
        }
        cmap_used = std::move(c2);
    }

    EdgeSet g = build_8conn_edges(L);
    if (g.edges.empty()) return cmap_used;
    const int ub = chromatic_upper_bound(g);

    // Build adjacency lists.
    const int numNodes = g.maxLabel;
    std::vector<std::set<int>> adj(static_cast<std::size_t>(numNodes + 1));
    for (auto [a, b] : g.edges) { adj[a].insert(b); adj[b].insert(a); }

    Value palette = select_maximally_distinct(cmap_used, ub, mr);
    const std::size_t paletteN = palette.dims().rows();
    const double *pd = palette.doubleData();

    // BFS starting from min label in L.
    int firstNode = std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < H * W; ++i) {
        int v = static_cast<int>(L.elemAsDouble(i));
        if (v > 0 && v < firstNode) firstNode = v;
    }
    if (firstNode == std::numeric_limits<int>::max()) return cmap_used;

    std::vector<int> coloredIdx(static_cast<std::size_t>(numNodes + 1), 0);
    std::vector<char> visited(static_cast<std::size_t>(numNodes + 1), 0);
    std::vector<int> queue;
    queue.reserve(static_cast<std::size_t>(numNodes));
    queue.push_back(firstNode);
    visited[firstNode] = 1;
    std::size_t head = 0;

    Value out = Value::matrix(cmap_used.dims().rows(), 3,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::fill(od, od + 3 * cmap_used.dims().rows(), 0.0);

    while (head < queue.size()) {
        int cur = queue[head++];
        // Enqueue unvisited neighbours.
        std::vector<int> visitedNeighbors;
        for (int n : adj[cur]) {
            if (!visited[n]) {
                visited[n] = 1;
                queue.push_back(n);
            } else {
                visitedNeighbors.push_back(n);
            }
        }
        int newIdx;
        if (head == 1) {           // first node → first palette colour
            newIdx = 1;
        } else {
            // Find lowest palette index not used by coloured neighbours.
            std::set<int> usedColors;
            for (int n : visitedNeighbors)
                if (coloredIdx[n] != 0) usedColors.insert(coloredIdx[n]);
            newIdx = 1;
            while (usedColors.count(newIdx)) ++newIdx;
            if (newIdx > static_cast<int>(paletteN))
                newIdx = static_cast<int>(paletteN);
        }
        coloredIdx[cur] = newIdx;
        const std::size_t N = cmap_used.dims().rows();
        od[0 * N + (cur - 1)] = pd[0 * paletteN + (newIdx - 1)];
        od[1 * N + (cur - 1)] = pd[1 * paletteN + (newIdx - 1)];
        od[2 * N + (cur - 1)] = pd[2 * paletteN + (newIdx - 1)];
    }
    // Labels that didn't appear in any edge keep cmap_used's row as fallback.
    const std::size_t N = cmap_used.dims().rows();
    const double *cd = cmap_used.doubleData();
    for (std::size_t i = 0; i < N; ++i) {
        // If the i-th label (i is row → label = i+1 wrt 1-based) was
        // never visited *and* it appears in L, leave the original colour.
        const int lab = static_cast<int>(i + 1);
        if (lab > numNodes) continue;
        if (!visited[lab]) {
            for (int ch = 0; ch < 3; ++ch)
                od[ch * N + i] = cd[ch * N + i];
        }
    }
    if (tf_zero) {
        // Remove the prepended zero-row (we put cmap(1,:) there).
        Value strip = Value::matrix(N - 1, 3, ValueType::DOUBLE, mr);
        double *sd = strip.doubleDataMut();
        for (int ch = 0; ch < 3; ++ch)
            for (std::size_t i = 1; i < N; ++i)
                sd[ch * (N - 1) + (i - 1)] = od[ch * N + i];
        return strip;
    }
    return out;
}

// ── Lowercase helper ──────────────────────────────────────────────
inline std::string lower(const std::string &s)
{
    std::string lo;
    lo.reserve(s.size());
    for (char ch : s)
        lo += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return lo;
}

// Parse a MATLAB color-spec string into an RGB triplet in [0,1], matching
// label2rgb's parseZerocolor (single-letter ColorSpec or full color name).
// Returns false if the string is not a recognized color.
inline bool parseColorSpec(const std::string &name, double rgb[3])
{
    const std::string lo = lower(name);
    auto set = [&](double r, double g, double b) {
        rgb[0] = r; rgb[1] = g; rgb[2] = b; return true;
    };
    if (lo == "y" || lo == "yellow")  return set(1, 1, 0);
    if (lo == "m" || lo == "magenta") return set(1, 0, 1);
    if (lo == "c" || lo == "cyan")    return set(0, 1, 1);
    if (lo == "r" || lo == "red")     return set(1, 0, 0);
    if (lo == "g" || lo == "green")   return set(0, 1, 0);
    if (lo == "b" || lo == "blue")    return set(0, 0, 1);
    if (lo == "w" || lo == "white")   return set(1, 1, 1);
    if (lo == "k" || lo == "black")   return set(0, 0, 0);
    return false;
}

}  // namespace

Value labeloverlay(const Value &A_in, const Value &L_in,
                   const Value &colormap,
                   const std::string &color_assignment,
                   const Value &included_labels,
                   double transparency,
                   std::pmr::memory_resource *mr)
{
    // ── Validate A ────────────────────────────────────────────────
    const auto &dA = A_in.dims();
    const std::size_t H = dA.rows();
    const std::size_t W = dA.cols();
    if (H == 0 || W == 0)
        throw Error("labeloverlay: A must be nonempty",
                    0, 0, "labeloverlay", "", "numkit:labeloverlay:emptyA");
    const bool isRGB = dA.is3D() && dA.pages() == 3;
    const bool isGray = !dA.is3D() || (dA.is3D() && dA.pages() == 1);
    if (!isRGB && !isGray)
        throw Error("labeloverlay: A must be grayscale (H×W) or RGB (H×W×3)",
                    0, 0, "labeloverlay", "", "numkit:labeloverlay:shapeA");

    // ── Validate L ────────────────────────────────────────────────
    const auto &dL = L_in.dims();
    if (dL.is3D() && dL.pages() != 1)
        throw Error("labeloverlay: L must be 2-D",
                    0, 0, "labeloverlay", "", "numkit:labeloverlay:shapeL");
    if (dL.rows() != H || dL.cols() != W)
        throw Error("labeloverlay: size(L) must match size(A,1:2)",
                    0, 0, "labeloverlay", "", "numkit:labeloverlay:sizeMismatch");

    // Logical → 0/1 integer label matrix. Already a numeric label
    // matrix? Accept as-is, but validate non-negative integer.
    const std::size_t plane = H * W;
    int maxLabel = 0;
    for (std::size_t i = 0; i < plane; ++i) {
        const double v = L_in.elemAsDouble(i);
        if (v < 0 || std::floor(v) != v || !std::isfinite(v))
            throw Error("labeloverlay: L must be non-negative integer-valued",
                        0, 0, "labeloverlay", "", "numkit:labeloverlay:badL");
        const int iv = static_cast<int>(v);
        if (iv > maxLabel) maxLabel = iv;
    }
    const int totalLabels = maxLabel + 1;

    // ── Transparency ──────────────────────────────────────────────
    if (!std::isfinite(transparency) || transparency < 0 || transparency > 1)
        throw Error("labeloverlay: Transparency must be in [0, 1]",
                    0, 0, "labeloverlay", "",
                    "numkit:labeloverlay:transparency");
    const double alphaVal = 1.0 - transparency;

    // ── Resolve colormap ──────────────────────────────────────────
    // colormap arg can be: empty (use 'jet') / numeric Nx3 / string name.
    Value cmap;
    bool cmap_was_string = false;
    if (colormap.isEmpty()) {
        cmap = jet_colormap(totalLabels, mr);
        cmap_was_string = true;
    } else if (colormap.isChar() || colormap.isString()) {
        cmap = resolve_named_colormap(colormap.toString(), totalLabels, mr);
        cmap_was_string = true;
    } else {
        if (colormap.dims().cols() != 3 || colormap.dims().is3D())
            throw Error("labeloverlay: Colormap must be an Nx3 numeric array",
                        0, 0, "labeloverlay", "",
                        "numkit:labeloverlay:cmapShape");
        // Convert to DOUBLE (matches MATLAB's normalizeColormap → single
        // then double for indexing). Stays bit-identical because the
        // user-supplied array IS the source of truth.
        cmap = Value::matrix(colormap.dims().rows(), 3,
                             ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < cmap.numel(); ++i)
            cmap.doubleDataMut()[i] = colormap.elemAsDouble(i);
    }

    // ── Resolve ColorAssignment ───────────────────────────────────
    std::string ca = lower(color_assignment.empty() ? "auto"
                                                     : color_assignment);
    if (ca == "auto") {
        ca = cmap_was_string ? "shuffle" : "noshuffle";
    } else if (ca != "shuffle" && ca != "noshuffle"
               && ca != "contrasting-neighbors") {
        throw Error("labeloverlay: ColorAssignment must be auto / shuffle / "
                    "noshuffle / contrasting-neighbors",
                    0, 0, "labeloverlay", "",
                    "numkit:labeloverlay:colorAssignment");
    }

    if (ca == "shuffle") {
        const int N = static_cast<int>(cmap.dims().rows());
        std::vector<int> p = matlab_default_randperm(N);
        cmap = permute_cmap_rows(cmap, p, mr);
    } else if (ca == "contrasting-neighbors") {
        cmap = greedy_graph_coloring(L_in, cmap, mr);
    }

    // ── Resolve IncludedLabels ────────────────────────────────────
    std::vector<int> included;
    if (included_labels.isEmpty()) {
        for (int k = 1; k <= maxLabel; ++k) included.push_back(k);
    } else {
        const std::size_t M = included_labels.numel();
        included.reserve(M);
        for (std::size_t i = 0; i < M; ++i) {
            const double v = included_labels.elemAsDouble(i);
            if (v < 0 || std::floor(v) != v || !std::isfinite(v))
                throw Error("labeloverlay: IncludedLabels must be "
                            "non-negative integers",
                            0, 0, "labeloverlay", "",
                            "numkit:labeloverlay:includedBad");
            included.push_back(static_cast<int>(v));
        }
        for (int v : included)
            if (v > maxLabel)
                throw Error("labeloverlay: IncludedLabels exceeds max label",
                            0, 0, "labeloverlay", "",
                            "numkit:labeloverlay:includedRange");
    }
    if (included.empty()) {
        // MATLAB behaviour: pass A through im2uint8.
        return im2uint8(A_in, mr);
    }
    if (static_cast<int>(cmap.dims().rows()) <
        static_cast<int>(included.size()))
        throw Error("labeloverlay: Colormap has fewer rows than the number "
                    "of labels",
                    0, 0, "labeloverlay", "",
                    "numkit:labeloverlay:cmapTooSmall");

    // ── Build alphamap ────────────────────────────────────────────
    bool zero_in_included = false;
    for (int v : included) if (v == 0) { zero_in_included = true; break; }
    std::vector<double> alphamap(cmap.dims().rows(), 0.0);
    if (zero_in_included) {
        for (int k : included) {
            const int idx = k + 1;
            if (idx - 1 < static_cast<int>(alphamap.size()))
                alphamap[idx - 1] = alphaVal;
        }
    } else {
        // alphamap(included) = alphaVal
        for (int k : included) {
            if (k - 1 < static_cast<int>(alphamap.size()))
                alphamap[k - 1] = alphaVal;
        }
        // cmap = [cmap(1,:); cmap]; alphamap = [0, alphamap].
        const std::size_t N = cmap.dims().rows();
        Value c2 = Value::matrix(N + 1, 3, ValueType::DOUBLE, mr);
        const double *cd = cmap.doubleData();
        double *cd2 = c2.doubleDataMut();
        for (int ch = 0; ch < 3; ++ch) {
            cd2[ch * (N + 1) + 0] = cd[ch * N + 0];
            for (std::size_t i = 0; i < N; ++i)
                cd2[ch * (N + 1) + (i + 1)] = cd[ch * N + i];
        }
        cmap = std::move(c2);
        alphamap.insert(alphamap.begin(), 0.0);
    }

    // ── Convert A to single in [0, 1] via im2single equivalent ────
    // We promote integer/logical inputs by class range; double/single
    // values are taken at face value (no rescale).
    auto a_pixel = [&](std::size_t r, std::size_t c, int ch) -> double {
        std::size_t off;
        if (isRGB) {
            off = static_cast<std::size_t>(ch) * H * W + c * H + r;
        } else {
            off = c * H + r;     // gray: same value across ch
        }
        const double raw = A_in.elemAsDouble(off);
        switch (A_in.type()) {
            case ValueType::UINT8:   return raw / 255.0;
            case ValueType::UINT16:  return raw / 65535.0;
            case ValueType::INT16: {
                // im2single for int16: (x + 32768) / 65535.
                return (raw + 32768.0) / 65535.0;
            }
            case ValueType::LOGICAL: return raw == 0.0 ? 0.0 : 1.0;
            default:                  return raw;   // double / single
        }
    };

    // ── Pixel-wise blend → uint8 ──────────────────────────────────
    Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
    uint8_t *od = out.uint8DataMut();
    const double *cd = cmap.doubleData();
    const std::size_t Nc = cmap.dims().rows();
    auto sat = [](double v) -> uint8_t {
        v = std::round(v * 255.0);
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        return static_cast<uint8_t>(v);
    };
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r) {
            const int lab = static_cast<int>(L_in.elemAsDouble(c * H + r));
            // MATLAB picks `alphamap(lab+1)` and `cmap(lab+1, :)` (1-based)
            // in both branches:
            //   • zero IN included: alphamap = zeros(N); alphamap(lab+1)=α.
            //     N == cmap rows (no prepend). C++ 0-based idx = lab.
            //   • zero NOT in included: alphamap = [0, alpha-padded];
            //     cmap = [cmap(1,:); cmap]. Both grow by 1; index still
            //     `lab + 1` in MATLAB ⇒ `lab` in C++.
            int aidx = lab;
            if (aidx < 0) aidx = 0;
            if (aidx >= static_cast<int>(alphamap.size()))
                aidx = static_cast<int>(alphamap.size()) - 1;
            const double a = alphamap[aidx];
            for (int ch = 0; ch < 3; ++ch) {
                const double Apx = a_pixel(r, c, ch);
                const double Cpx = cd[ch * Nc + aidx];
                const double blended = (1.0 - a) * Apx + a * Cpx;
                od[static_cast<std::size_t>(ch) * H * W + c * H + r] =
                    sat(blended);
            }
        }
    return out;
}

namespace detail {

void labeloverlay_reg(Span<const Value> args, std::size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("labeloverlay: requires (A, L [, NV...])",
                    0, 0, "labeloverlay", "",
                    "numkit:labeloverlay:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    Value cmap;             // empty → default 'jet'
    std::string ca = "auto";
    Value included;         // empty → default 1:maxLabel
    double transparency = 0.5;

    std::size_t i = 2;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("labeloverlay: expected NV-pair name string",
                        0, 0, "labeloverlay", "",
                        "numkit:labeloverlay:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "colormap") {
            cmap = args[i + 1];
        } else if (nlo == "colorassignment") {
            ca = args[i + 1].toString();
        } else if (nlo == "includedlabels") {
            included = args[i + 1];
        } else if (nlo == "transparency") {
            transparency = args[i + 1].toScalar();
        } else {
            throw Error("labeloverlay: unknown option '" + name + "'",
                        0, 0, "labeloverlay", "",
                        "numkit:labeloverlay:unknownNv");
        }
        i += 2;
    }

    outs[0] = labeloverlay(args[0], args[1], cmap, ca, included,
                           transparency, mr);
}

// label2rgb(L [, map [, zerocolor [, order]]]) — MATLAB R2025b parity.
//   map       : omitted/[] → jet(numregion); colormap NAME string; or Nx3.
//   zerocolor : omitted → white; RGB triplet; or a ColorSpec string
//               ('y/m/c/r/g/b/w/k' or full color names).
//   order     : 'noshuffle' (default). 'shuffle' deferred (needs the
//               swb2712 RNG stream).
// numregion = max(L(:)); the colormap is generated with that many rows, so
// label k maps to row k and label 0 maps to zerocolor. Delegates the pixel
// mapping to the existing uint8 label2rgb(L, cmap, zerocolor) core.
void label2rgb_reg(Span<const Value> args, std::size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("label2rgb: requires (L [, map [, zerocolor [, order]]])",
                    0, 0, "label2rgb", "", "numkit:label2rgb:nargin");
    auto *mr = ctx.engine->resource();
    const Value &L = args[0];

    // numregion = max(L(:)); force ≥1 so the generated colormap always has
    // 3 columns (a 1-row map is never indexed when L is all background).
    const std::size_t plane = L.numel();
    int maxLabel = 0;
    for (std::size_t i = 0; i < plane; ++i) {
        const double v = L.elemAsDouble(i);
        if (std::isfinite(v) && v > maxLabel) maxLabel = static_cast<int>(v);
    }
    const int numregion = maxLabel < 1 ? 1 : maxLabel;

    // ── Resolve colormap: default jet / named string / Nx3 matrix ──
    Value cmap;
    if (args.size() < 2 || args[1].isEmpty())
        cmap = jet_colormap(numregion, mr);
    else if (args[1].isChar() || args[1].isString())
        cmap = resolve_named_colormap(args[1].toString(), numregion, mr);
    else
        cmap = args[1];                       // explicit Nx3 — core validates.

    // ── order (4th positional): only 'noshuffle' supported ─────────
    if (args.size() >= 4 && (args[3].isChar() || args[3].isString())) {
        const std::string ord = lower(args[3].toString());
        if (ord == "shuffle")
            throw Error("label2rgb: order 'shuffle' is not yet supported "
                        "(requires MATLAB's swb2712 random stream)",
                        0, 0, "label2rgb", "",
                        "numkit:label2rgb:shuffleUnsupported");
        if (ord != "noshuffle")
            throw Error("label2rgb: order must be 'noshuffle' or 'shuffle'",
                        0, 0, "label2rgb", "", "numkit:label2rgb:order");
    }

    // ── Resolve zerocolor: RGB triplet or ColorSpec string ─────────
    Value bg;                                 // empty → core defaults white.
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (args[2].isChar() || args[2].isString()) {
            double rgb[3];
            if (!parseColorSpec(args[2].toString(), rgb))
                throw Error("label2rgb: invalid zerocolor string '" +
                            args[2].toString() + "'",
                            0, 0, "label2rgb", "",
                            "numkit:label2rgb:zerocolor");
            bg = Value::matrix(1, 3, ValueType::DOUBLE, mr);
            double *bd = bg.doubleDataMut();
            bd[0] = rgb[0]; bd[1] = rgb[1]; bd[2] = rgb[2];
        } else {
            bg = args[2];                     // numeric triplet — core checks.
        }
    }

    outs[0] = label2rgb(L, cmap, bg, mr);
}

} // namespace detail
} // namespace numkit::image
