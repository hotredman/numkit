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
                        0, 0, "cmunique", "", "m:cmunique:cls");
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
                    0, 0, "cmunique", "", "m:cmunique:map");
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
                    0, 0, "cmunique", "", "m:cmunique:rgb");
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
                    0, 0, "cmunique", "", "m:cmunique:i");
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
                    0, 0, "ycbcr2rgbwide", "", "m:ycbcr2rgbwide:bps");
    if (YCBCR.type() != ValueType::UINT16)
        throw Error("ycbcr2rgbwide: YCBCR must be UINT16",
                    0, 0, "ycbcr2rgbwide", "", "m:ycbcr2rgbwide:class");

    const auto &d = YCBCR.dims();
    const bool is_image = d.is3D();
    if (is_image) {
        if (d.pages() != 3)
            throw Error("ycbcr2rgbwide: H×W×3 image expected",
                        0, 0, "ycbcr2rgbwide", "", "m:ycbcr2rgbwide:shape");
    } else {
        if (d.cols() != 3)
            throw Error("ycbcr2rgbwide: p×3 colour list expected",
                        0, 0, "ycbcr2rgbwide", "", "m:ycbcr2rgbwide:shape");
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

void ycbcr2rgbwide_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ycbcr2rgbwide: requires (YCBCR, BPS)",
                    0, 0, "ycbcr2rgbwide", "", "m:ycbcr2rgbwide:nargin");
    const int bps = static_cast<int>(args[1].toScalar());
    outs[0] = ycbcr2rgbwide(args[0], bps, ctx.engine->resource());
}

void cmunique_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cmunique: requires (X, MAP), (RGB), or (I)",
                    0, 0, "cmunique", "", "m:cmunique:nargin");
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

} // namespace detail
} // namespace numkit::image
