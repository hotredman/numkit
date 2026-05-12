// libs/image/include/numkit/image/color/color.hpp
//
// Colour space conversions. All accept either an H×W×3 image (matching
// MATLAB) or an N×3 colormap (rows = pixels, cols = channels).
// Output class:
//   - rgb2hsv / hsv2rgb / rgb2ycbcr / ycbcr2rgb / rgb2lab / lab2rgb
//     all return double in MATLAB; we follow the same convention.
//   - rgb2xyz / xyz2rgb same.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <vector>

namespace numkit::image {

/// Split a multi-plane image into per-plane Values (`imsplit(I, planes)`).
///
/// Splits an H×W×P volume into P planes (H×W each). For 2-D input
/// returns a single H×W copy in `planes[0]`. `planes` is resized to P;
/// output planes share the input's class.
///
/// @param I       Input image.
/// @param planes  Output vector — resized in-place.
/// @param mr      Memory resource (nullptr → process default).
void imsplit(const Value &I, std::vector<Value> &planes,
             std::pmr::memory_resource *mr = nullptr);

/// RGB → HSV (`hsv = rgb2hsv(rgb)`).
///
/// Accepts either H×W×3 image or N×3 colormap. Output is double:
/// H ∈ [0, 1] (hue normalised), S ∈ [0, 1], V ∈ [0, 1]. Float input
/// in [0, 1] is taken at face value; integer input is rescaled by
/// class range first.
/// @see hsv2rgb
Value rgb2hsv  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// HSV → RGB inverse of @ref rgb2hsv.
Value hsv2rgb  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// RGB → YCbCr (BT.601) — Y, Cb, Cr packaged like the input (H×W×3 or N×3).
/// Output is double. @see ycbcr2rgb
Value rgb2ycbcr(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Inverse of @ref rgb2ycbcr.
Value ycbcr2rgb(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// RGB → NTSC YIQ. Y ∈ [0, 1], I / Q ∈ [-1, 1] roughly.
/// @see ntsc2rgb
Value rgb2ntsc (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Inverse of @ref rgb2ntsc.
Value ntsc2rgb (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// sRGB → CIE XYZ (D65). Applies sRGB linearisation then the standard
/// 3×3 matrix transform. Output is double XYZ. @see xyz2rgb
Value rgb2xyz  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// CIE XYZ → sRGB inverse of @ref rgb2xyz.
Value xyz2rgb  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// sRGB → CIE L*a*b* (D65). Internally goes through @ref rgb2xyz
/// then @ref xyz2lab. Output is double Lab in MATLAB-canonical scale
/// (L* ∈ [0, 100], a*, b* ∈ [-128, 127] roughly).
/// @see lab2rgb, rgb2lightness
Value rgb2lab  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// CIE L*a*b* → sRGB inverse of @ref rgb2lab.
Value lab2rgb  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// rgb2lightness(RGB) — lightness L (= L* of CIE Lab). Returns H×W
/// single image. Equivalent to the first plane of rgb2lab(RGB).
Value rgb2lightness(const Value &RGB, std::pmr::memory_resource *mr = nullptr);

/// rgb2ind with a fixed input colormap: nearest-neighbour quantization
/// (squared Euclidean in normalized RGB). Output index is uint8 if cmap
/// has ≤ 256 rows, else uint16. KNOWN GAP: scalar-Q (min-variance) and
/// scalar-tol (uniform) forms deferred.
std::pair<Value, Value>
rgb2ind_inmap(const Value &RGB, const Value &cmap, std::pmr::memory_resource *mr = nullptr);
/// CIE XYZ → CIE L*a*b* with the D50 / ICC reference white. @see lab2xyz
Value xyz2lab  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// CIE L*a*b* → CIE XYZ inverse of @ref xyz2lab.
Value lab2xyz  (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// CIE L*a*b* class conversion helpers.
///
/// Scale conventions:
///   - double / single: L* ∈ [0, 100], a*/b* ∈ [-128, 127].
///   - uint8:  L* ∈ [0, 255],   a*/b* ∈ [0, 255]   with offset 128.
///   - uint16: L* ∈ [0, 65280], a*/b* ∈ [0, 65280] with offset 32768.
///
/// Each helper dispatches on input class and rescales + offsets
/// per-channel; output keeps the input's shape (M×3 colormap or
/// M×N×3 image).
///@{
Value lab2double (const Value &lab, std::pmr::memory_resource *mr = nullptr);
Value lab2single (const Value &lab, std::pmr::memory_resource *mr = nullptr);
Value lab2uint8  (const Value &lab, std::pmr::memory_resource *mr = nullptr);
Value lab2uint16 (const Value &lab, std::pmr::memory_resource *mr = nullptr);
///@}

/// `M = colorgradient(C [, w] [, n])` — colormap that smoothly
/// traverses the K-by-3 anchor RGB colors `C` with relative segment
/// weights `w` (length K-1, default ones) into `n` output rows
/// (default 64). Per-segment linspace; output is double N×3.
Value colorgradient(const Value &C, const Value &w, int n, std::pmr::memory_resource *mr = nullptr);

/// `RGB = wavelength2rgb(wavelength [, class [, gamma]])` —
/// piecewise visible-light wavelength → RGB mapping (Bruton 1996).
/// Scalar input returns a 1×3 row; 1-D vector returns
/// 1×N×3 / N×1×3; 2-D matrix returns H×W×3. `class` defaults to
/// "double" and `gamma` to 0.8. Output class follows `class`
/// through the existing im2* helpers.
Value wavelength2rgb(const Value &wavelength, const std::string &out_class, double gamma, std::pmr::memory_resource *mr = nullptr);

/// colorangle(rgb1, rgb2) — angle in degrees between two RGB
/// colours: rad2deg(acos(dot(rgb1, rgb2) / (|rgb1|·|rgb2|))).
/// Inputs may be 3-element vectors (any orientation) or N×3
/// matrices; broadcasting between a single colour and an N×3
/// stack is supported. Returns 0 when both colours are zero, NaN
/// when only one is zero. The cosine is clamped to [−1, 1] to
/// guard against floating-point drift on identical colours.
Value colorangle(const Value &rgb1, const Value &rgb2, std::pmr::memory_resource *mr = nullptr);

/// `map = gray([n])` — N×3 grayscale colormap. Default n = 256 (we
/// don't track figure state). n == 1 → [0, 0, 0]; n ≤ 0 → 0×3.
/// Otherwise gr = (0:n-1)/(n-1), repeated across all 3 channels.
Value gray_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = hot([n])` — N×3 black-red-yellow-white colormap.
/// Default n = 256. Octave behaviour: n==1 → [1 1 1]; n==2 →
/// [1 1 0.5; 1 1 1]; n>2 piecewise R-then-G-then-B ramps with
/// idx=floor(3/8·n). n ≤ 0 → 0×3.
Value hot_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = cool([n])` — N×3 cyan-to-magenta colormap. Default n=256.
/// r = (0:n-1)/(n-1); g = 1 - r; b = 1. n==1 → [0 1 1]; n ≤ 0 → 0×3.
Value cool_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = spring([n])` — N×3 magenta-to-yellow colormap. Default n=256.
/// r = 1; g = (0:n-1)/(n-1); b = 1 - g. n==1 → [1 0 1]; n ≤ 0 → 0×3.
Value spring_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = summer([n])` — N×3 green-to-yellow colormap. Default n=256.
/// r = (0:n-1)/(n-1); g = 0.5 + r/2; b = 0.4. n==1 → [0 0.5 0.4];
/// n ≤ 0 → 0×3.
Value summer_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = autumn([n])` — N×3 red-to-yellow colormap. Default n=256.
/// r = 1; g = (0:n-1)/(n-1); b = 0. n==1 → [1 0 0]; n ≤ 0 → 0×3.
Value autumn_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = winter([n])` — N×3 blue→cyan-ish colormap. Default n=256.
/// r = 0; g = (0:n-1)/(n-1); b = 1 - g/2. n==1 → [0 0 1]; n ≤ 0 → 0×3.
Value winter_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = copper([n])` — N×3 copper-tinted colormap. Default n=256.
/// x = (0:n-1)/(n-1); r = min(5/4·x, 1); g = 0.7812·x; b = 0.4975·x.
/// n==1 → [0 0 0]; n ≤ 0 → 0×3.
Value copper_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = pink([n])` — N×3 pastel-pink colormap. Default n=256.
/// idx = floor(3/8·n); piecewise linspace ramps for R/G/B then take
/// the element-wise sqrt to lift saturation. n==1 → sqrt([1/3 1/3 1/3]);
/// n==2 → sqrt([1/3 1/3 1/6; 1 1 1]); n ≤ 0 → 0×3.
Value pink_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = hsv([n])` — N×3 hue-rotation colormap. Default n=256. Equivalent
/// to `hsv2rgb([(0:n-1)'/n, 1, 1])`. n==1 → [1 0 0]; n ≤ 0 → 0×3.
Value hsv_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = flag([n])` — N×3 cyclic red/white/blue/black colormap.
/// Rows cycle through the 4-row pattern `[1 0 0; 1 1 1; 0 0 1; 0 0 0]`.
/// Default n = 256. n ≤ 0 → 0×3.
Value flag_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = prism([n])` — N×3 cyclic 6-row rainbow palette
/// `[red, orange, yellow, green, blue, violet]` (`[1 0 0; 1 0.5 0;
/// 1 1 0; 0 1 0; 0 0 1; 2/3 0 1]`). Default n = 256. n ≤ 0 → 0×3.
Value prism_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = lines([n])` — N×3 colormap that cycles through MATLAB's
/// default axes color order (R2025b 7-row palette). n==1 → [0 0 1]
/// (MATLAB convention); n ≤ 0 → 0×3. Default n = 256. NB: Octave's
/// default palette differs slightly; we match MATLAB.
Value lines_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = bone([n])` — N×3 grayscale-with-blue-tint "bone" colormap.
/// Per Octave's bone.m: x=(0:n-1)/(n-1) and per-channel piecewise
/// linspace ramps with idx_R=floor(3/4·n), idx_G=floor(3/8·n).
/// `base` adjustment depends on mod(n,8) (cases {2,4}, {5,7},
/// otherwise). Default n = 256. n==1 → [1/8 1/8 1/8];
/// n==2 → [1/16 1/8 1/8; 1 1 1]; n ≤ 0 → 0×3.
Value bone_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `map = white([n])` — N×3 all-ones colormap. Default n=256.
/// n ≤ 0 → 0×3.
Value white_cmap(int n, std::pmr::memory_resource *mr = nullptr);

/// `B = rgb2lin(A)` — sRGB → linear-RGB gamma inverse.
/// Per-element piecewise: |x| < 0.04045 → x/12.92; otherwise
/// sign(x) · ((|x|+0.055)/1.055)^2.4. Negatives are mirrored
/// through the gamma curve. Output class is double if input is
/// double, else single (MATLAB R2025b convention). Only the
/// default "sRGB" colorspace is supported here.
Value rgb2lin(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// `B = lin2rgb(A)` — linear-RGB → sRGB gamma forward.
/// Per-element piecewise: |x| ≤ 0.0031308 → 12.92·x; otherwise
/// sign(x) · (1.055·|x|^(1/2.4) − 0.055). Inverse of rgb2lin.
/// Same class-promotion rule as rgb2lin (integer in → single out).
Value lin2rgb(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// `xyzd = xyz2double(xyz)` — convert XYZ image/colormap to double.
/// Following the ICC.1:2001-4 convention for uint16 XYZ:
///     uint16 0 → 0.0,  uint16 32768 → 1.0,  uint16 65535 → 1+32767/32768.
/// Equivalently `double = uint16 / 32768`. Double input is passed through
/// (sanitized to double class). Shape is preserved (M×3 or H×W×3).
Value xyz2double(const Value &xyz, std::pmr::memory_resource *mr = nullptr);

/// `xyzu16 = xyz2uint16(xyz)` — convert XYZ to uint16 ICC encoding.
/// Inverse of xyz2double: `uint16 = saturate(round(double * 32768))`.
/// Negative values clip to 0; values ≥ 65535/32768 ≈ 1.99997 saturate
/// to 65535. uint16 input is passed through. Shape preserved.
Value xyz2uint16(const Value &xyz, std::pmr::memory_resource *mr = nullptr);

/// `rmap = brighten(map, beta)` — gamma-adjust an N×3 colormap.
/// `beta` ∈ (−1, 1). Output is `map .^ gamma` where gamma = 1−β
/// for β>0 (brighter) or gamma = 1/(1+β) for β≤0 (darker).
/// Range outside the open interval errors.
Value brighten(const Value &map, double beta, std::pmr::memory_resource *mr = nullptr);

/// `cmap = contrast(x[, m])` — gray colormap that equalises image
/// histogram. Per MATLAB R2025b: scale x to [0, m-1] integers,
/// concat with [0..m], sort, return rising-edge positions divided
/// by their max as a length-≈m gray colormap. Three identical
/// columns. Default `m` = 64 in our impl (no figure colormap to
/// inherit). Octave ships a similar function but its output
/// disagrees with MATLAB's; we follow MATLAB.
Value contrast(const Value &x, int m, std::pmr::memory_resource *mr = nullptr);

/// `delE = deltaE(I1, I2[, 'isInputLab', tf])` — CIE76 colour
/// difference. Default treats inputs as RGB and converts to L*a*b*
/// internally; pass `isInputLab=true` to skip the conversion.
/// Result is sqrt(sum((Lab1 - Lab2).^2, 3)) — Euclidean distance
/// in CIELAB. Inputs are M×3 colormaps (output M×1) or H×W×3
/// images (output H×W). Class promotion: any double → double,
/// otherwise single.
Value deltaE(const Value &I1, const Value &I2, bool isInputLab, std::pmr::memory_resource *mr = nullptr);

/// `wp = whitepoint([illuminant])` — 1×3 XYZ tristimulus value of
/// a CIE reference illuminant. Supported (case-insensitive):
///   'a'   → [1.0985 1.0000 0.3558]      (Tungsten 2856 K)
///   'c'   → [0.9807 1.0000 1.1823]      (Average daylight)
///   'd50' → [0.96419866 1.0 0.82511648] (Horizon)
///   'd55' → [0.9568 1.0 0.9214]         (Mid-morning daylight)
///   'd65' → [0.95047 1.0 1.08883]       (Noon daylight)
///   'e'   → [1 1 1]                     (Equal-energy)
///   'icc' → [0.96420288 1.0 0.82489014] (default; ICC profile D50)
Value whitepoint(const std::string &illuminant, std::pmr::memory_resource *mr = nullptr);

/// `gmap = cmap2gray(cmap)` — colormap → grayscale colormap.
/// Input is an N×3 RGB colormap (treated as double). Output is N×3
/// double, where each row is `[y y y]` and y is the luminance from
/// the inv(YIQ→RGB) first row weights (0.298936, 0.587043, 0.114021),
/// clipped to [0, 1]. Matches MATLAB R2020b+. Octave-image 2.18.2
/// doesn't ship cmap2gray.
Value cmap2gray(const Value &cmap, std::pmr::memory_resource *mr = nullptr);

/// label2rgb(L, cmap [, background]) — colourise a labelled image.
/// `L` is an H×W non-negative integer-valued matrix. `cmap` is an
/// N×3 colormap (double in [0, 1]). Pixels with label == 0 take the
/// `background` color (default [1, 1, 1] = white). Output is H×W×3
/// uint8.
///
/// The full MATLAB signature accepts a colormap-name string or a
/// function handle for `cmap`; both require a `jet` / `hsv` / etc.
/// generator that we don't expose yet, so callers must pass an
/// explicit N×3 matrix here.
Value label2rgb(const Value &L, const Value &cmap, const Value &background, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
