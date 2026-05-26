// libs/image/include/numkit/image/color/color.hpp
//
// Colour space conversions and colormaps.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <vector>

namespace numkit::image {

/// @file
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

} // namespace numkit::image
