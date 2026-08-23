// toolboxes/image/include/numkit/image/color/color.hpp
//
// Colour space conversions and colormaps.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <vector>

namespace numkit::image {

/// @addtogroup group_image
/// @{


/// @file
/// @ingroup group_image
/// @brief Colour-space conversions, Lab helpers, colormaps.
///
/// **Image shape convention.** Most colour-space conversions accept
/// either an `H × W × 3` image or an `N × 3` colormap
/// (rows = pixels, cols = channels).
///
/// **Output class.** `rgb2*` / `*2rgb` / `rgb2lab` / `lab2rgb` /
/// `rgb2xyz` / `xyz2rgb` return DOUBLE.
///
/// **Integer input.** When the input is integer-typed, values are
/// rescaled by the class range first; double / single inputs in
/// `[0, 1]` are taken at face value.

/// @brief Split a multi-plane image into per-plane Values
/// (`imsplit(I, planes)`).
///
/// Splits an `H × W × P` volume into `P` planes (`H × W` each). For
/// 2-D input returns a single `H × W` copy in `planes[0]`. `planes`
/// is resized to `P`; output planes share the input's class.
///
/// @param I       Input image.
/// @param planes  Output vector (resized in-place).
/// @param mr      Memory resource (nullptr → process default).
void imsplit(const Value &I, std::vector<Value> &planes,
             std::pmr::memory_resource *mr = nullptr);

// ── RGB ↔ HSV / YCbCr / NTSC / XYZ / Lab ─────────────────────────────

/// @brief RGB → HSV (`hsv = rgb2hsv(rgb)`).
///
/// Output is DOUBLE: `H ∈ [0, 1]` (hue normalised), `S ∈ [0, 1]`,
/// `V ∈ [0, 1]`.
///
/// @param x   Input RGB image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    HSV image / colormap (DOUBLE).
/// @see hsv2rgb
Value rgb2hsv(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief HSV → RGB (inverse of @ref rgb2hsv).
/// @param x   HSV image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    RGB image / colormap (DOUBLE).
Value hsv2rgb(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief RGB → YCbCr (BT.601) (`ycbcr = rgb2ycbcr(rgb)`).
/// @param x   RGB image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    YCbCr image / colormap (DOUBLE).
/// @see ycbcr2rgb
Value rgb2ycbcr(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief YCbCr → RGB (inverse of @ref rgb2ycbcr).
/// @param x   YCbCr image / colormap. @param mr  Memory resource.
/// @return    RGB image / colormap (DOUBLE).
Value ycbcr2rgb(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief RGB → NTSC YIQ (`yiq = rgb2ntsc(rgb)`).
///
/// `Y ∈ [0, 1]`, `I` and `Q ∈ [-1, 1]` approximately.
///
/// @param x   RGB image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    YIQ image / colormap.
/// @see ntsc2rgb
Value rgb2ntsc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief NTSC YIQ → RGB (inverse of @ref rgb2ntsc).
/// @param x   YIQ image / colormap. @param mr  Memory resource.
/// @return    RGB image / colormap.
Value ntsc2rgb(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief sRGB → CIE XYZ (D65) (`xyz = rgb2xyz(rgb)`).
///
/// Applies sRGB linearisation then the standard 3×3 matrix transform.
///
/// @param x   sRGB image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    XYZ image / colormap (DOUBLE).
/// @see xyz2rgb, rgb2lin
Value rgb2xyz(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief CIE XYZ → sRGB (inverse of @ref rgb2xyz).
/// @param x   XYZ image / colormap. @param mr  Memory resource.
/// @return    sRGB image / colormap (DOUBLE).
Value xyz2rgb(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief sRGB → CIE L\*a\*b\* (D65) (`lab = rgb2lab(rgb)`).
///
/// Internally routes through @ref rgb2xyz → @ref xyz2lab. Output is
/// DOUBLE Lab in the canonical scale: `L* ∈ [0, 100]`,
/// `a*, b* ∈ [-128, 127]` roughly.
///
/// @param x   sRGB image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Lab image / colormap.
/// @see lab2rgb, rgb2lightness
Value rgb2lab(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief CIE L\*a\*b\* → sRGB (inverse of @ref rgb2lab).
/// @param x   Lab image / colormap. @param mr  Memory resource.
/// @return    sRGB image / colormap.
Value lab2rgb(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Lightness L\* (`L = rgb2lightness(RGB)`).
///
/// Returns an `H × W` SINGLE image. Equivalent to the first plane of
/// @ref rgb2lab.
///
/// @param RGB  sRGB image.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Lightness image.
Value rgb2lightness(const Value &RGB, std::pmr::memory_resource *mr = nullptr);

/// @brief Eliminate duplicate colors in colormap; convert
/// grayscale or RGB to indexed (`[Y, newmap] = cmunique(...)`).
///
/// Three input signatures:
///   * `cmunique(X, MAP)`   — quantise `MAP` to 1/1024 and drop
///                             duplicate rows; rebuild `X` to
///                             reference the compressed map.
///   * `cmunique(RGB)`      — treat each pixel as a distinct
///                             colour, then run the dedup pass.
///   * `cmunique(I)`        — treat each intensity as the grey
///                             triplet `[I I I]`, then dedup.
///
/// The output index `Y` is `uint8` if `newmap` has ≤ 256 rows and
/// `double` otherwise (matches MATLAB R2025b).
///
/// @param X    Indexed image (uint8 / uint16 / double / single).
/// @param MAP  `N × 3` double colormap.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Pair `(Y, newmap)` with the smallest equivalent
///             colormap.
std::pair<Value, Value>
cmunique_xm(const Value &X, const Value &MAP,
            std::pmr::memory_resource *mr = nullptr);
std::pair<Value, Value>
cmunique_rgb(const Value &RGB,
             std::pmr::memory_resource *mr = nullptr);
std::pair<Value, Value>
cmunique_i(const Value &I,
           std::pmr::memory_resource *mr = nullptr);

/// @brief RGB → indexed image with a fixed colormap
/// (`[ind, cmap] = rgb2ind(RGB, cmap)`).
///
/// Nearest-neighbour quantisation (squared Euclidean in normalised
/// RGB). Output index is UINT8 if `cmap` has `≤ 256` rows, else UINT16.
/// KNOWN GAP: scalar-Q (min-variance) and scalar-tol (uniform) forms
/// deferred.
///
/// @param RGB   Source image.
/// @param cmap  `K × 3` colormap.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(ind, cmap)` pair.
std::pair<Value, Value>
rgb2ind_inmap(const Value &RGB, const Value &cmap, std::pmr::memory_resource *mr = nullptr);

/// @brief CIE XYZ → CIE L\*a\*b\* (D50 ICC reference white).
/// @param x   XYZ image / colormap. @param mr  Memory resource.
/// @return    Lab image / colormap. @see lab2xyz, rgb2lab
Value xyz2lab(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief CIE L\*a\*b\* → CIE XYZ (inverse of @ref xyz2lab).
/// @param x   Lab image / colormap. @param mr  Memory resource.
/// @return    XYZ image / colormap.
Value lab2xyz(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Lab class conversions ───────────────────────────────────────────
//
// **Scale conventions:**
// - DOUBLE / SINGLE: `L* ∈ [0, 100]`, `a*/b* ∈ [-128, 127]`.
// - UINT8:  `L* ∈ [0, 255]`, `a*/b* ∈ [0, 255]` with offset 128.
// - UINT16: `L* ∈ [0, 65280]`, `a*/b* ∈ [0, 65280]` with offset 32768.

/// @brief Cast Lab image to DOUBLE (`labd = lab2double(lab)`).
/// @param lab  Input Lab.  @param mr  Memory resource.
/// @return     DOUBLE Lab in canonical scale.
/// @see lab2single, lab2uint8, lab2uint16
Value lab2double(const Value &lab, std::pmr::memory_resource *mr = nullptr);

/// @brief Cast Lab image to SINGLE (`labs = lab2single(lab)`).
/// @param lab  Input Lab.  @param mr  Memory resource.
/// @return     SINGLE Lab in canonical scale.
Value lab2single(const Value &lab, std::pmr::memory_resource *mr = nullptr);

/// @brief Cast Lab image to UINT8 (`labu = lab2uint8(lab)`).
/// @param lab  Input Lab.  @param mr  Memory resource.
/// @return     UINT8 Lab with offset 128.
Value lab2uint8(const Value &lab, std::pmr::memory_resource *mr = nullptr);

/// @brief Cast Lab image to UINT16 (`labu = lab2uint16(lab)`).
/// @param lab  Input Lab.  @param mr  Memory resource.
/// @return     UINT16 Lab with offset 32768.
Value lab2uint16(const Value &lab, std::pmr::memory_resource *mr = nullptr);

// ── Colormap synthesis ──────────────────────────────────────────────

/// @brief Smooth colormap through K anchor colours
/// (`M = colorgradient(C, w, n)`).
///
/// Per-segment linspace between consecutive rows of `C`, with relative
/// segment weights `w` (length `K - 1`, default ones). Output is DOUBLE
/// `N × 3`.
///
/// @param C   `K × 3` anchor RGB colours.
/// @param w   Segment weights vector (or `Value::Empty` → ones).
/// @param n   Output rows (default 64).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `N × 3` colormap.
Value colorgradient(const Value &C, const Value &w, int n,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Wavelength → RGB (Bruton 1996)
/// (`RGB = wavelength2rgb(wavelength, class, gamma)`).
///
/// Piecewise visible-light mapping. Scalar input → `1 × 3`; vector →
/// `1 × N × 3` / `N × 1 × 3`; matrix → `H × W × 3`.
///
/// @param wavelength  Wavelength(s) in nanometres.
/// @param out_class   Output class (default `"double"`).
/// @param gamma       Gamma correction (default 0.8).
/// @param mr          Memory resource (nullptr → process default).
/// @return            RGB image / colormap of the requested class.
Value wavelength2rgb(const Value &wavelength, const std::string &out_class,
                     double gamma, std::pmr::memory_resource *mr = nullptr);

/// @brief Angle between two RGB colours
/// (`theta = colorangle(rgb1, rgb2)`).
///
/// `rad2deg(acos(dot(rgb1, rgb2) / (|rgb1| · |rgb2|)))`. Inputs may be
/// 3-element vectors or `N × 3` matrices; broadcasting between a single
/// colour and a stack is supported. Returns 0 when both colours are
/// zero, `NaN` when only one is zero. Cosine is clamped to `[-1, 1]`.
///
/// @param rgb1  First RGB.
/// @param rgb2  Second RGB.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Angle(s) in degrees.
Value colorangle(const Value &rgb1, const Value &rgb2,
                 std::pmr::memory_resource *mr = nullptr);

// ── Standard colormaps ───────────────────────────────────────
//
// All follow the convention:
// - `n` defaults to 256 in adapters (we don't track figure state).
// - `n == 1` → palette's "extreme" row.
// - `n <= 0` → `0 × 3` empty.
// - Output is DOUBLE `N × 3`.

/// @brief Grayscale colormap (`map = gray(n)`).
/// `gr = (0:n-1)/(n-1)` repeated across all 3 channels.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value gray_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Black-red-yellow-white colormap (`map = hot(n)`).
/// Piecewise R-then-G-then-B ramps with `idx = floor(3/8 · n)`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value hot_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Cyan → magenta colormap (`map = cool(n)`).
/// `r = (0:n-1)/(n-1)`, `g = 1 - r`, `b = 1`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value cool_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Magenta → yellow colormap (`map = spring(n)`).
/// `r = 1`, `g = (0:n-1)/(n-1)`, `b = 1 - g`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value spring_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Green → yellow colormap (`map = summer(n)`).
/// `r = (0:n-1)/(n-1)`, `g = 0.5 + r/2`, `b = 0.4`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value summer_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Red → yellow colormap (`map = autumn(n)`).
/// `r = 1`, `g = (0:n-1)/(n-1)`, `b = 0`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value autumn_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Blue → cyan-ish colormap (`map = winter(n)`).
/// `r = 0`, `g = (0:n-1)/(n-1)`, `b = 1 - g/2`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value winter_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Copper-tinted colormap (`map = copper(n)`).
/// `x = (0:n-1)/(n-1)`; `r = min(5/4·x, 1)`, `g = 0.7812·x`,
/// `b = 0.4975·x`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value copper_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Pastel-pink colormap (`map = pink(n)`).
/// Piecewise linspace ramps for R/G/B then elementwise `sqrt` for
/// saturation lift.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value pink_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Hue-rotation colormap (`map = hsv(n)`).
/// Equivalent to `hsv2rgb([(0:n-1)'/n, 1, 1])`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value hsv_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Cyclic red/white/blue/black colormap (`map = flag(n)`).
/// Cycles `[1 0 0; 1 1 1; 0 0 1; 0 0 0]`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value flag_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Cyclic 6-row rainbow palette (`map = prism(n)`).
/// `[red, orange, yellow, green, blue, violet]`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value prism_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Axes-color-order palette (`map = lines(n)`).
/// Cycles the 7-row default colour order. `n == 1 → [0 0 1]`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value lines_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Grayscale-with-blue-tint colormap (`map = bone(n)`).
/// Per-channel piecewise ramps with `idx_R = floor(3/4 · n)`,
/// `idx_G = floor(3/8 · n)`.
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` colormap.
Value bone_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// @brief All-ones colormap (`map = white(n)`).
/// @param n   Row count. @param mr  Memory resource.
/// @return    `n × 3` all-ones colormap.
Value white_cmap(int n, std::pmr::memory_resource *mr = nullptr);

// ── Gamma helpers / XYZ class conversion / brighten / contrast ──────

/// @brief sRGB → linear RGB gamma inverse (`B = rgb2lin(A)`).
///
/// Per-element piecewise: `|x| < 0.04045 → x / 12.92`; otherwise
/// `sign(x) · ((|x| + 0.055) / 1.055)^2.4`. Negatives are mirrored
/// through the gamma curve. Output class is DOUBLE if input is DOUBLE,
/// else SINGLE. Only `"sRGB"` colourspace supported.
///
/// @param A   Input image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Linearised image / colormap.
/// @see lin2rgb
Value rgb2lin(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Linear RGB → sRGB gamma forward (`B = lin2rgb(A)`).
///
/// Per-element piecewise: `|x| <= 0.0031308 → 12.92 · x`; otherwise
/// `sign(x) · (1.055 · |x|^(1/2.4) - 0.055)`. Same class-promotion
/// rule as @ref rgb2lin.
///
/// @param A   Input image / colormap.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Gamma-encoded image / colormap.
/// @see rgb2lin
Value lin2rgb(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief XYZ → DOUBLE (`xyzd = xyz2double(xyz)`).
///
/// Follows the ICC.1:2001-4 convention: UINT16 `32768 → 1.0`. DOUBLE
/// input passes through. Shape preserved.
///
/// @param xyz  Input XYZ image / colormap (DOUBLE or UINT16).
/// @param mr   Memory resource (nullptr → process default).
/// @return     DOUBLE XYZ.
/// @see xyz2uint16
Value xyz2double(const Value &xyz, std::pmr::memory_resource *mr = nullptr);

/// @brief XYZ → UINT16 ICC encoding (`xyzu16 = xyz2uint16(xyz)`).
///
/// Inverse of @ref xyz2double: `uint16 = saturate(round(double · 32768))`.
/// Negative values clip to 0; values `>= 65535/32768 ≈ 1.99997` saturate
/// to 65535. UINT16 input passes through.
///
/// @param xyz  Input XYZ image / colormap.
/// @param mr   Memory resource (nullptr → process default).
/// @return     UINT16 XYZ.
Value xyz2uint16(const Value &xyz, std::pmr::memory_resource *mr = nullptr);

/// @brief Gamma-adjust a colormap (`rmap = brighten(map, beta)`).
///
/// `gamma = 1 - β` for `β > 0` (brighter), `gamma = 1/(1 + β)` for
/// `β <= 0` (darker). `β` outside `(-1, 1)` throws.
///
/// @param map   Input `N × 3` colormap.
/// @param beta  Gamma adjustment in `(-1, 1)`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Adjusted colormap.
/// @throws Error  `beta` out of range.
Value brighten(const Value &map, double beta,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Histogram-equalising gray colormap (`cmap = contrast(x, m)`).
///
/// Algorithm: scale `x` to `[0, m-1]` integers, concat with
/// `[0..m]`, sort, return rising-edge positions divided by their max
/// as a length-≈`m` gray colormap. Three identical columns.
///
/// @param x   Input image used to derive the histogram.
/// @param m   Target colormap length (default 64 in adapter).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `≈m × 3` gray colormap.
Value contrast(const Value &x, int m, std::pmr::memory_resource *mr = nullptr);

// ── Colour difference / whitepoints / utility ───────────────────────

/// @brief BT.2020 / BT.2100 wide-gamut RGB → YCbCr
/// (`ycbcr = rgbwide2ycbcr(RGB, bps)`).
///
/// Non-constant-luminance YCbCr per ITU-R BT.2020-2 / BT.2100-2
/// (10-bit or 12-bit narrow-range). `bps` must be 10 or 12. Input
/// `RGB` is UINT16 with values in `[64, 940]` (10-bit) or
/// `[256, 3760]` (12-bit); out-of-range pixels are mapped via the
/// same affine transform but the result is no longer guaranteed to
/// land in the nominal YCbCr range.
///
/// Algorithm:
///   * Normalise: `rgbN = (RGB − blackLevel) / nominalRange`
///   * `Y'  = 0.2627·R + 0.6780·G + 0.0593·B`
///   * `Cb = (B − Y')/1.8814,  Cr = (R − Y')/1.4746`
///   * Quantise: `Y_out  = uint16((219·Y'  + 16 )·2^(bps−8))`,
///               `Cb_out = uint16((224·Cb + 128)·2^(bps−8))`,
///               `Cr_out = uint16((224·Cr + 128)·2^(bps−8))`
///
/// Accepts `p × 3` colour lists (returned as `p × 3`) and
/// `H × W × 3` images.
///
/// @param RGB             UINT16 wide-gamut RGB.
/// @param bits_per_sample 10 or 12.
/// @param mr              Memory resource (nullptr → process default).
/// @return                UINT16 YCbCr, same shape as `RGB`.
Value rgbwide2ycbcr(const Value &RGB, int bits_per_sample,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief BT.2020 / BT.2100 wide-gamut YCbCr → RGB
/// (`rgb = ycbcr2rgbwide(YCBCR, bps)`).
///
/// Inverse of @ref rgbwide2ycbcr. Decodes non-constant-luminance
/// YCbCr per ITU-R BT.2020-2 / BT.2100-2 narrow-range. `bps` must be
/// 10 or 12. Input is UINT16 in the YCbCr nominal ranges:
///   * Y  ∈ `[64, 940]` (10-bit) or `[256, 3760]` (12-bit)
///   * Cb, Cr ∈ `[64, 960]` (10-bit) or `[256, 3840]` (12-bit)
///
/// Algorithm (matches MATLAB R2025b `ycbcr2rgbwideImpl.m`):
///   * Normalise:
///       `Y_n  = (Y  − yzero      )/yrange`
///       `Cb_n = (Cb − chromazero)/chromarange`
///       `Cr_n = (Cr − chromazero)/chromarange`
///     where `yzero = 64/256`, `chromazero = 2^(bps − 1)`,
///     `yrange = peak − zero`, `chromarange = (peak_chroma) − zero`.
///   * `R = 1.4746·Cr_n + Y_n,   B = 1.8814·Cb_n + Y_n`
///     `G = (Y_n − 0.2627·R − 0.0593·B) / 0.6780`
///   * Quantise: `RGB_out = uint16(rgb·nominalRange + blackLevel)`
///     where `blackLevel = 64/256`, `nominalRange = peak − black`.
///
/// Accepts `p × 3` colour lists and `H × W × 3` images. Bit-equal
/// MATLAB round-trip with @ref rgbwide2ycbcr.
///
/// @param YCBCR           UINT16 YCbCr per BT.2020 narrow-range.
/// @param bits_per_sample 10 or 12.
/// @param mr              Memory resource (nullptr → process default).
/// @return                UINT16 RGB, same shape as `YCBCR`.
Value ycbcr2rgbwide(const Value &YCBCR, int bits_per_sample,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief CIE94 / CIEDE2000 colour difference
/// (`dE = imcolordiff(I1, I2, ...)`).
///
/// **Default standard = CIE94.** Pass `standard = "CIEDE2000"` to use
/// the 2000 revision. Inputs may be:
///   * `c × 3` colour lists (`c × 1` output);
///   * `H × W × 3` images (`H × W` output);
///   * higher-dim arrays where the 3rd dim is the colour channel
///     (output preserves all other dims, dropping the channel axis).
///
/// **Weighting factors** (textile / graphic-arts defaults):
///   * `kL`, `kC`, `kH` — parametric factors for L*, C*, H* (default 1).
///   * `K1`, `K2` — CIE94 chroma / hue weighting constants
///     (defaults 0.045 and 0.015 — the graphic-arts setting). For
///     textile applications use `K1 = 0.048`, `K2 = 0.014`.
///
/// `is_input_lab = false` (default) → inputs are RGB and are converted
/// to L*a*b* with @ref rgb2lab. `true` → inputs are already Lab and
/// the conversion is skipped (only DOUBLE / SINGLE inputs allowed in
/// that case).
///
/// Reference: ISO 11664-6:2014 (CIE2000) and CIE Publ. 116-1995
/// (CIE94). MATLAB R2025b `images.color.internal.deltaE94` /
/// `deltaE2000` — verified bit-equal on probe outputs.
///
/// @param I1            First colour image / list.
/// @param I2            Second colour image / list (same shape).
/// @param standard      "CIE94" or "CIEDE2000".
/// @param is_input_lab  Skip the RGB→Lab step.
/// @param kL            L\* weighting factor (must be > 0).
/// @param kC            C\* weighting factor (must be > 0).
/// @param kH            H\* weighting factor (must be > 0).
/// @param K1            CIE94 chroma constant.
/// @param K2            CIE94 hue constant.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Per-pixel colour difference (channel dim removed).
Value imcolordiff(const Value &I1, const Value &I2,
                  const std::string &standard, bool is_input_lab,
                  double kL, double kC, double kH, double K1, double K2,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief CIE76 colour difference (`delE = deltaE(I1, I2, isInputLab)`).
///
/// Default treats inputs as RGB and converts to L\*a\*b\* internally;
/// pass `isInputLab = true` to skip the conversion. Result is
/// `sqrt(sum((Lab1 - Lab2).^2, 3))` — Euclidean distance in CIELAB.
/// Class promotion: any DOUBLE input → DOUBLE output, else SINGLE.
///
/// @param I1           First image (RGB by default, or Lab).
/// @param I2           Second image (matching class / shape).
/// @param isInputLab   When `true`, inputs are Lab; skip conversion.
/// @param mr           Memory resource (nullptr → process default).
/// @return             `H × W` (or `N × 1`) deltaE.
Value deltaE(const Value &I1, const Value &I2, bool isInputLab,
             std::pmr::memory_resource *mr = nullptr);

/// @brief CIE reference-illuminant tristimulus (`wp = whitepoint(illuminant)`).
///
/// Supported (case-insensitive): `"a"` (Tungsten 2856 K), `"c"`
/// (Average daylight), `"d50"` (Horizon), `"d55"`, `"d65"` (Noon),
/// `"e"` (Equal-energy), `"icc"` (default, ICC profile D50).
///
/// @param illuminant  Illuminant code.
/// @param mr          Memory resource (nullptr → process default).
/// @return            `1 × 3` XYZ tristimulus.
/// @throws Error  Unknown illuminant code.
Value whitepoint(const std::string &illuminant,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief RGB colormap → grayscale colormap (`gmap = cmap2gray(cmap)`).
///
/// Each output row is `[y y y]` where `y` is the YIQ luminance
/// `0.298936·R + 0.587043·G + 0.114021·B` clipped to `[0, 1]`.
///
/// @param cmap  `N × 3` RGB colormap.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `N × 3` grayscale colormap.
Value cmap2gray(const Value &cmap, std::pmr::memory_resource *mr = nullptr);

// ── White-balance illumination estimation ─────────────────────────────

/// @brief White-Patch illuminant estimate
/// (`illum = illumwhite(A [, P] [, 'Mask', M])`).
///
/// Returns a 1×3 RGB row-vector approximating the scene illuminant.
///
/// **Algorithm.** With `P == 0`, returns per-channel max over masked
/// pixels (classical White-Patch retinex, Land & McCann 1971). With
/// `P > 0`, returns the per-channel mean of the top-`P`% pixels by
/// L2 norm of `(R, G, B)` (top-percentile variant, Banić & Lončarić
/// 2014). MATLAB default `P = 1`.
///
/// @param A     `H × W × 3` numeric image.
/// @param P     Percentile in `[0, 100)`. Default 1 (top 1 %).
/// @param mask  Optional `H × W` logical/numeric mask; empty → use all.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `1 × 3` DOUBLE illuminant.
Value illumwhite(const Value &A, double P, const Value &mask,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief PCA-based illuminant estimate
/// (`illum = illumpca(A [, P] [, 'Mask', M])`).
///
/// Returns a 1×3 RGB row-vector approximating the scene illuminant
/// using the dominant direction of the darkest and brightest `P`% of
/// pixels (Cheng-Prasad-Brown, JOSA A 31(5), 2014, p. 1049-1058).
///
/// **Algorithm.** Pixels are ordered by the magnitude of their
/// projection on the mean colour direction. The top-`P`% brightest
/// and bottom-`P`% darkest pixels (default `P = 3.5`) are kept; PCA
/// (SVD of the not-mean-centred 3-column matrix) gives the principal
/// direction, which is returned as `abs(V(:,1))` so the illuminant
/// always lives in the first octant.
///
/// **Degenerate case** (single colour, `V == I`, or near-equal
/// singular values): the mean of the selected colours is returned
/// instead, matching MATLAB's source.
///
/// @param A     `H × W × 3` numeric image.
/// @param P     Percentile in `(0, 50]`. Default 3.5. `P >= 50`
///              uses all pixels.
/// @param mask  Optional `H × W` mask; empty → all.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `1 × 3` DOUBLE illuminant.
Value illumpca(const Value &A, double P, const Value &mask,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Grey-World illuminant estimate
/// (`illum = illumgray(A [, P] [, 'Mask', M])`).
///
/// Returns a 1×3 RGB row-vector approximating the scene illuminant.
///
/// **Algorithm.** Sorts the masked pixels by L2 norm of `(R, G, B)`,
/// optionally trims the bottom `p_lo`% and top `p_hi`%, then returns
/// the per-channel mean of the survivors (Grey-World hypothesis,
/// Buchsbaum 1980). `P` is either a scalar (applied to both ends) or
/// a 2-element vector `[p_lo, p_hi]`, each in `[0, 50)`. MATLAB default
/// `P = 0` (no trimming).
///
/// @param A     `H × W × 3` numeric image.
/// @param P     Percentile(s); empty vector → P=0.
/// @param mask  Optional `H × W` mask; empty → all.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `1 × 3` DOUBLE illuminant.
Value illumgray(const Value &A, const std::vector<double> &P,
                const Value &mask, std::pmr::memory_resource *mr = nullptr);

/// @brief Colourise a labelled image
/// (`RGB = label2rgb(L, cmap, background)`).
///
/// `L` is an `H × W` non-negative integer-valued matrix. `cmap` is an
/// `N × 3` colormap (DOUBLE in `[0, 1]`). Pixels with label `== 0`
/// take the `background` colour (default `[1, 1, 1]` = white).
///
/// **Scope.** The full signature accepts a colormap-name
/// string or a function handle for `cmap`; both require a `jet` /
/// `hsv` / etc. generator that we don't expose yet, so callers must
/// pass an explicit `N × 3` matrix here.
///
/// @param L           Labelled image.
/// @param cmap        `N × 3` colormap.
/// @param background  Background colour (`Value::Empty` → white).
/// @param mr          Memory resource (nullptr → process default).
/// @return            `H × W × 3` UINT8 colour image.
Value label2rgb(const Value &L, const Value &cmap, const Value &background,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Composite two images for visual comparison
/// (`C = imfuse(A, B, method, ...)`).
///
/// Builds a UINT8 composite of `A` and `B` for visual comparison
/// (e.g. registration QA). Five fusion methods (default
/// `"falsecolor"`):
///
///   * `"falsecolor"` — convert each input to grayscale, scale,
///     then assign to RGB channels per `ColorChannels` (default
///     `"green-magenta"` = `[2 1 2]`).
///   * `"blend"`      — alpha-blend at 50/50 in overlap; pass-
///     through outside overlap (no spatial-ref form here, so the
///     overlap is the whole image).
///   * `"diff"`       — `|A - B|` as scaled grayscale.
///   * `"checkerboard"` — alternating 8x8 super-blocks from `A`/`B`
///     (block tile resized to image size by nearest neighbour).
///   * `"montage"`    — `[A B]` side-by-side concatenation.
///
/// `Scaling` controls grayscale conversion:
///   * `"independent"` (default) — scale each input to `[0, 1]`
///     before `im2uint8`.
///   * `"joint"` — concatenate, scale together, then split.
///   * `"none"` — no rescale (just class cast).
///
/// `ColorChannels` for `"falsecolor"` only: 3-element vector
/// `[R G B]` with values in `{0, 1, 2}` (image index, 0 = neither)
/// or the named shortcuts `"red-cyan"` = `[1 2 2]`, or
/// `"green-magenta"` = `[2 1 2]` (default).
///
/// If `A` and `B` differ in `H × W`, both are zero-padded to the
/// element-wise max along each dimension before fusion.
///
/// Spatial referencing (the `(A, RA, B, RB)` form using `imref2d`)
/// is **not** supported — `imref2d` is a MATLAB OOP class and §0 of
/// the library policy forbids us from implementing it. The two-
/// output form `[C, RC]` is therefore not exposed.
///
/// Output is always UINT8 with `class(C) = uint8`.
///
/// @param A         First image (grayscale 2-D, RGB 2-D x 3, or logical).
/// @param B         Second image (any numeric class).
/// @param method    `"falsecolor"` (def) / `"blend"` / `"diff"` /
///                  `"checkerboard"` / `"montage"`.
/// @param scaling   `"independent"` (def) / `"joint"` / `"none"`.
/// @param channels  3-element vector in `{0,1,2}` for `"falsecolor"`
///                  (default `[2 1 2]` = green-magenta).
/// @param mr        Memory resource (nullptr → process default).
/// @return          UINT8 composite image.
Value imfuse(const Value &A, const Value &B,
             const std::string &method,
             const std::string &scaling,
             const Value &channels,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Render high-dynamic-range image for low-dynamic-range
/// display (`RGB = tonemap(HDR, ...)`).
///
/// Ward's log-luminance equalisation followed by adaptive
/// histogram equalisation + lightness/saturation remap. Output is
/// always UINT8.
///
/// Pipeline (MATLAB R2025b tonemap.m):
///   1. Replace zeros with the global non-zero minimum.
///   2. RGBlog2 = log2(HDR);  RGBlog2Scaled = mat2gray(RGBlog2).
///   3. Grayscale path: adapthisteq(NumTiles=ntiles) → imadjust(LRemap, [0 1]).
///      RGB path: rgb2lab → L/100 → adapthisteq → imadjust → ·100;
///      a/b channels multiplied by saturation; lab2rgb.
///   4. im2uint8(result).
///
/// References: G. Ward et al., "A Visibility Matching Tone
/// Reproduction Operator for High Dynamic Range Scenes", IEEE
/// TVCG 3(4), 1997.
///
/// @param HDR        2-D grayscale or H×W×3 single/double HDR
///                   image (nonnegative).
/// @param lremap_lo  AdjustLightness low (∈ [0, 1]). Default 0.
/// @param lremap_hi  AdjustLightness high (∈ [0, 1]). Default 1.
/// @param saturation AdjustSaturation (≥ 0). Default 1.
/// @param ntilesR    NumberOfTiles rows (default 4).
/// @param ntilesC    NumberOfTiles cols (default 4).
/// @param mr         Memory resource (nullptr → process default).
/// @return           UINT8 LDR image, same H×W×{1,3} as HDR.
Value tonemap(const Value &HDR,
              double lremap_lo, double lremap_hi,
              double saturation,
              int ntilesR, int ntilesC,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Overlay a label / mask / categorical-index image on a 2-D base
/// (`B = labeloverlay(A, L, ...)`).
///
/// Each non-zero label `k > 0` is painted with a colour
/// `cmap(k+1, :)` blended into the base via
/// `B = (1-α)·A + α·cmap`, where `α = 1 - transparency`. Label 0
/// (or labels not in `included_labels`) passes the base through
/// untouched. Output is always UINT8 (`im2uint8` conversion at the
/// end), and always 3-channel — grayscale `A` is replicated to RGB.
///
/// Pipeline (MATLAB R2025b labeloverlay.m + LabelColormapHelper.m):
///   1. `A = im2single(A)`. Replicate to 3 planes if grayscale.
///   2. `maxLabel = max(L(:))`, `totalLabels = maxLabel + 1`.
///   3. Resolve colormap:
///      - `"jet"` (default) → `cmap = jet(totalLabels)`.
///      - Named string `"hsv"`/`"parula"`/... → `feval(name, totalLabels)`.
///      - Numeric `Nx3` → used as-is.
///   4. ColorAssignment:
///      - `Auto` → `NoShuffle` if `colormap` was numeric, else `Shuffle`.
///      - `Shuffle` → permute cmap rows by `randperm(N)` with
///        MATLAB-canonical Mersenne-Twister seed 0 (bit-identical
///        with `rng('default'); randperm(N)`).
///      - `ContrastingNeighbors` → greedy graph colouring of the
///        8-connected adjacency graph of `L`.
///   5. Build `alphamap`. If `0 ∈ included_labels`: `alphamap(k+1)=α`
///      for each included `k`. Else: insert a dummy row at the top of
///      `cmap` (= `cmap(1,:)`) and `0` at the front of `alphamap`, so
///      label 0 paints the base untouched.
///   6. Per pixel: `B(r,c,ch) = (1-alphamap(L+1)) · A(r,c,ch)
///      + alphamap(L+1) · cmap(L+1,ch)`.
///   7. `im2uint8(B)`.
///
/// Inputs may be grayscale (`H × W`) or RGB (`H × W × 3`). Logical
/// masks are treated as a label matrix with values in `{0, 1}`. The
/// `categorical` input form is **not** supported because `categorical`
/// is a MATLAB OOP class (see policy §0); use a `uint8`/`uint16`
/// label matrix instead.
///
/// @param A                  Base image (uint8/uint16/int16/single/double).
/// @param L                  Label / mask matrix (non-negative integer).
/// @param colormap           Colormap. Pass `Value::Empty` for default
///                           `"jet"`. May be a numeric `Nx3` array
///                           or a string name (`"jet"`, `"hsv"`,
///                           `"parula"`, `"hot"`, `"cool"`, `"spring"`,
///                           `"summer"`, `"autumn"`, `"winter"`,
///                           `"gray"`, `"bone"`, `"copper"`, `"pink"`,
///                           `"lines"`, `"colorcube"`, `"prism"`,
///                           `"flag"`).
/// @param color_assignment   `"auto"` (def) / `"shuffle"` /
///                           `"noshuffle"` / `"contrasting-neighbors"`.
/// @param included_labels    Pass `Value::Empty` for default
///                           `1:maxLabel`. Otherwise a vector of
///                           non-negative integers.
/// @param transparency       Alpha blend ∈ [0, 1]. `0` = opaque
///                           colour, `1` = invisible (default 0.5).
/// @param mr                 Memory resource (nullptr → process default).
/// @return                   UINT8 `H × W × 3` overlay image.
Value labeloverlay(const Value &A, const Value &L,
                   const Value &colormap,
                   const std::string &color_assignment,
                   const Value &included_labels,
                   double transparency,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Chromatic adaptation for white-balance correction
/// (`B = chromadapt(A, illuminant, ...)`).
///
/// Rebalances RGB image colours under a known scene illuminant by
/// mapping responses from a "source" white-point (the illuminant) to
/// CIE D65 (the destination assumed by sRGB and most other
/// device-RGB encodings). Three documented methods:
///
///   * **Bradford** (default; Lam 1985) — linear LMS-cone-space
///     adaptation using the Bradford matrix. The CIECAM02 Bradford
///     variant is the de-facto standard.
///   * **von Kries** — Hunt-Pointer-Estevez sharpened LMS basis,
///     simpler than Bradford but less perceptually uniform.
///   * **Simple** — per-channel RGB scaling (no LMS transform).
///     Fast but approximate; useful when the gamut transformation
///     would clip badly.
///
/// Pipeline (Bradford / vonKries):
///   1. Normalize illuminant: `illuminant_xyz = rgb2xyz(illuminant);
///      illuminant_xyz /= illuminant_xyz(2)` (Y=1).
///   2. Build adaptation matrix
///      `M_adapt = M⁻¹ · diag(M·whiteD65 / M·illuminant) · M`
///      where M is Bradford or von Kries.
///   3. `A_XYZ = rgb2xyz(A)` (with D65 reference).
///   4. `B_XYZ = M_adapt · A_XYZ` per pixel.
///   5. `B = xyz2rgb(B_XYZ)` (D65 reference).
///
/// Pipeline (Simple):
///   1. `illuminant_xyz` as above.
///   2. `illuminant_rgb = xyz2rgb(illuminant_xyz)`.
///   3. `B(:,:,c) = A(:,:,c) / illuminant_rgb(c)` (per channel).
///
/// `ColorSpace` controls the gamma + primaries:
///   * `"srgb"` (default) — sRGB primaries, sRGB piecewise gamma.
///   * `"adobe-rgb-1998"` — Adobe primaries, γ ≈ 2.2.
///   * `"prophoto-rgb"` — ProPhoto primaries with D50 white,
///     piecewise gamma (γ = 1.8 + linear toe).
///   * `"linear-rgb"` — sRGB primaries, identity gamma.
///
/// Output class matches input (`uint8` / `uint16` / `single` /
/// `double`).
///
/// @param A             RGB image (`H × W × 3`, uint8/uint16/single/double).
/// @param illuminant    3-element numeric vector in the *same* RGB
///                      colour space as A (NOT XYZ).
/// @param method        `"bradford"` (def) / `"vonkries"` / `"simple"`.
/// @param color_space   `"srgb"` (def) / `"adobe-rgb-1998"` /
///                      `"prophoto-rgb"` / `"linear-rgb"`.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Adapted RGB image, same class as `A`.
///
/// References:
///   - Lam, K.M. (1985). Metamerism and Colour Constancy. PhD thesis,
///     University of Bradford.
///   - Hunt, R. W. G. (2005). The Reproduction of Colour, 6th ed.
Value chromadapt(const Value &A, const Value &illuminant,
                 const std::string &method,
                 const std::string &color_space,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Narrow-range wide-gamut RGB → CIE 1931 XYZ
/// (`XYZ = rgbwide2xyz(RGB, BPS, ...)`).
///
/// Decodes BT.2020 or BT.2100 RGB values to CIE 1931 XYZ tristimulus
/// values. Pipeline:
///
///   1. Remove black-level offset and scale to nominal [0, 1]:
///      `lin = (raw - blackLevel) / (nominalPeak - blackLevel)`.
///   2. Inverse transfer function:
///      - BT.2020 (default), 10-bit: α = 1.099, β = 0.018.
///      - BT.2020, 12-bit:           α = 1.0993, β = 0.0181.
///      - BT.2100 PQ:                α = 1.099, β = 0.018 (hardcoded
///        — MATLAB's "PQ" path actually applies the BT.2020-style
///        transfer with fixed α/β regardless of bit depth, NOT the
///        SMPTE ST 2084 PQ curve).
///      - BT.2100 HLG: piecewise (v² / 3) below 1/2, (exp((v-c)/a) +
///        b) / 12 above. Constants a = 0.17883277, b = 1 - 4a,
///        c = 0.5 - a·ln(4a).
///   3. Matrix transform RGB → XYZ via BT.2020 primaries (D65):
///      `M = [0.636958 0.144617 0.168881;
///            0.262700 0.677998 0.059302;
///            0.000000 0.028073 1.060985]` (xr=0.708/yr=0.292,
///      xg=0.170/yg=0.797, xb=0.131/yb=0.046 with D65).
///   4. Optional Bradford chromatic adaptation if `whitepoint` ≠ D65.
///
/// Bit depths supported: 10, 12.
///
/// References:
///   - ITU-R Rec. BT.2020-2 (10/2015).
///   - ITU-R Rec. BT.2100-2 (07/2018).
///
/// @param RGB           Narrow-range uint16 values (10-bit: [64,940];
///                      12-bit: [256,3760]). Shape `H × W × 3` or
///                      `N × 3`.
/// @param bits_per_sample  10 or 12.
/// @param color_space      `"BT.2020"` (def) / `"BT.2100"`.
/// @param linearization    `"PQ"` (def) / `"HLG"` — BT.2100 only.
/// @param mr               Memory resource.
/// @return                 DOUBLE XYZ values.
Value rgbwide2xyz(const Value &RGB, int bits_per_sample,
                  const std::string &color_space,
                  const std::string &linearization,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief CIE 1931 XYZ → narrow-range wide-gamut RGB
/// (`RGB = xyz2rgbwide(XYZ, BPS, ...)`).
///
/// Inverse of @ref rgbwide2xyz. Same color-space and transfer-function
/// options. Returns uint16 narrow-range values clipped to the nominal
/// peak / black levels.
///
/// @param XYZ           DOUBLE XYZ values, shape `H × W × 3` or `N × 3`.
/// @param bits_per_sample  10 or 12.
/// @param color_space      `"BT.2020"` (def) / `"BT.2100"`.
/// @param linearization    `"PQ"` (def) / `"HLG"` — BT.2100 only.
/// @param mr               Memory resource.
/// @return                 UINT16 narrow-range RGB.
Value xyz2rgbwide(const Value &XYZ, int bits_per_sample,
                  const std::string &color_space,
                  const std::string &linearization,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Convert a Bayer-encoded mosaic to a truecolor RGB image
/// (`RGB = demosaic(I, sensorAlignment, BitsPerSample=bps)`).
///
/// Implements the high-quality linear interpolation of Malvar, He, and
/// Cutler (ICASSP 2004): five 5×5 integer kernels (G-at-RB,
/// R/B-at-G-same-row, R/B-at-G-diff-row, R-at-B / B-at-R) reconstruct
/// the missing channels at each pixel. At the sensor positions
/// themselves the raw mosaic value is returned unchanged.
///
/// Boundary handling reflects through the FIRST pixel
/// (`k=-1 → orig(1)`, `k=N → orig(N-2)`) so the mirrored
/// neighbourhood preserves the Bayer pattern — this differs from
/// `imfilter`'s standard `symmetric` mode and matches MATLAB R2025b.
///
/// @param I                Bayer mosaic (`M×N`, uint8 / uint16 /
///                         uint32). `M`, `N` must both be even.
/// @param sensorAlignment  One of `"rggb"`, `"bggr"`, `"grbg"`,
///                         `"gbrg"` — colour of the (1,1) pixel and
///                         its right neighbour.
/// @param bitsPerSample    Optional bits-per-sample for clamping the
///                         output (e.g. 12 for 12-bit data in a
///                         uint16 container). 0 means use the class
///                         maximum (default).
/// @param mr               Memory resource (nullptr → process default).
/// @return                 `M×N×3` truecolor image of the input class.
///
/// References:
///   - Malvar, He, Cutler. "High-Quality Linear Interpolation for
///     Demosaicing of Bayer-Patterned Color Images", IEEE ICASSP 2004.
Value demosaic(const Value &I, const std::string &sensorAlignment,
               int bitsPerSample = 0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Deinterleave a Bayer CFA mosaic into its four sensor planes
/// (`P = raw2planar(cfa)`).
///
/// Splits an `M×N` mosaic into an `(M/2)×(N/2)×4` array where the
/// channels are, in order, the (odd-row, odd-col), (odd-row, even-col),
/// (even-row, odd-col), (even-row, even-col) sub-samples. No
/// interpolation; pure parity-based copy. Class preserved.
///
/// @param cfa  `M×N` numeric / logical mosaic. `M`, `N` must be even.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(M/2)×(N/2)×4` array of the input class.
Value raw2planar(const Value &cfa, std::pmr::memory_resource *mr = nullptr);

/// @brief Re-interleave 4 sensor planes into a full Bayer CFA mosaic
/// (`cfa = planar2raw(I)`).
///
/// Inverse of @ref raw2planar. Takes an `(M)×(N)×4` array and
/// produces a `(2M)×(2N)` mosaic by inverse parity-based copy.
/// Class preserved.
///
/// @param I   `M×N×4` numeric / logical sensor-plane array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(2M)×(2N)` mosaic of the input class.
Value planar2raw(const Value &I, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::image
