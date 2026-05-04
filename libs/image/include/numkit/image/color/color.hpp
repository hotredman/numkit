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

/// imsplit(I) — split an H×W×P volume into P planes (H×W each).
/// For 2-D input returns a single H×W copy in planes[0]. Output
/// vector is resized to P; output planes share the input's class.
void imsplit(std::pmr::memory_resource *mr,
             const Value &I, std::vector<Value> &planes);

Value rgb2hsv  (std::pmr::memory_resource *mr, const Value &x);
Value hsv2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value rgb2ycbcr(std::pmr::memory_resource *mr, const Value &x);
Value ycbcr2rgb(std::pmr::memory_resource *mr, const Value &x);

Value rgb2ntsc (std::pmr::memory_resource *mr, const Value &x);
Value ntsc2rgb (std::pmr::memory_resource *mr, const Value &x);

Value rgb2xyz  (std::pmr::memory_resource *mr, const Value &x);
Value xyz2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value rgb2lab  (std::pmr::memory_resource *mr, const Value &x);
Value lab2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value xyz2lab  (std::pmr::memory_resource *mr, const Value &x);
Value lab2xyz  (std::pmr::memory_resource *mr, const Value &x);

/// L*a*b* class conversion: L* in [0, 100] (or 0..255 / 0..65280
/// integer), a*/b* in [-128, 127] (or [0, 65280] for uint16). The
/// helpers below dispatch on input class and rescale + offset
/// per-channel; output keeps the input's shape (Mx3 colormap or
/// MxNx3 image).
Value lab2double (std::pmr::memory_resource *mr, const Value &lab);
Value lab2single (std::pmr::memory_resource *mr, const Value &lab);
Value lab2uint8  (std::pmr::memory_resource *mr, const Value &lab);
Value lab2uint16 (std::pmr::memory_resource *mr, const Value &lab);

/// `M = colorgradient(C [, w] [, n])` — colormap that smoothly
/// traverses the K-by-3 anchor RGB colors `C` with relative segment
/// weights `w` (length K-1, default ones) into `n` output rows
/// (default 64). Per-segment linspace; output is double N×3.
Value colorgradient(std::pmr::memory_resource *mr,
                    const Value &C, const Value &w, int n);

/// `RGB = wavelength2rgb(wavelength [, class [, gamma]])` —
/// piecewise visible-light wavelength → RGB mapping (Bruton 1996).
/// Scalar input returns a 1×3 row; 1-D vector returns
/// 1×N×3 / N×1×3; 2-D matrix returns H×W×3. `class` defaults to
/// "double" and `gamma` to 0.8. Output class follows `class`
/// through the existing im2* helpers.
Value wavelength2rgb(std::pmr::memory_resource *mr,
                     const Value &wavelength,
                     const std::string &out_class,
                     double gamma);

/// colorangle(rgb1, rgb2) — angle in degrees between two RGB
/// colours: rad2deg(acos(dot(rgb1, rgb2) / (|rgb1|·|rgb2|))).
/// Inputs may be 3-element vectors (any orientation) or N×3
/// matrices; broadcasting between a single colour and an N×3
/// stack is supported. Returns 0 when both colours are zero, NaN
/// when only one is zero. The cosine is clamped to [−1, 1] to
/// guard against floating-point drift on identical colours.
Value colorangle(std::pmr::memory_resource *mr,
                 const Value &rgb1, const Value &rgb2);

/// `map = gray([n])` — N×3 grayscale colormap. Default n = 256 (we
/// don't track figure state). n == 1 → [0, 0, 0]; n ≤ 0 → 0×3.
/// Otherwise gr = (0:n-1)/(n-1), repeated across all 3 channels.
Value gray_cmap(std::pmr::memory_resource *mr, int n);

/// `map = hot([n])` — N×3 black-red-yellow-white colormap.
/// Default n = 256. Octave behaviour: n==1 → [1 1 1]; n==2 →
/// [1 1 0.5; 1 1 1]; n>2 piecewise R-then-G-then-B ramps with
/// idx=floor(3/8·n). n ≤ 0 → 0×3.
Value hot_cmap(std::pmr::memory_resource *mr, int n);

/// `map = cool([n])` — N×3 cyan-to-magenta colormap. Default n=256.
/// r = (0:n-1)/(n-1); g = 1 - r; b = 1. n==1 → [0 1 1]; n ≤ 0 → 0×3.
Value cool_cmap(std::pmr::memory_resource *mr, int n);

/// `map = spring([n])` — N×3 magenta-to-yellow colormap. Default n=256.
/// r = 1; g = (0:n-1)/(n-1); b = 1 - g. n==1 → [1 0 1]; n ≤ 0 → 0×3.
Value spring_cmap(std::pmr::memory_resource *mr, int n);

/// `map = summer([n])` — N×3 green-to-yellow colormap. Default n=256.
/// r = (0:n-1)/(n-1); g = 0.5 + r/2; b = 0.4. n==1 → [0 0.5 0.4];
/// n ≤ 0 → 0×3.
Value summer_cmap(std::pmr::memory_resource *mr, int n);

/// `map = autumn([n])` — N×3 red-to-yellow colormap. Default n=256.
/// r = 1; g = (0:n-1)/(n-1); b = 0. n==1 → [1 0 0]; n ≤ 0 → 0×3.
Value autumn_cmap(std::pmr::memory_resource *mr, int n);

/// `map = winter([n])` — N×3 blue→cyan-ish colormap. Default n=256.
/// r = 0; g = (0:n-1)/(n-1); b = 1 - g/2. n==1 → [0 0 1]; n ≤ 0 → 0×3.
Value winter_cmap(std::pmr::memory_resource *mr, int n);

/// `map = copper([n])` — N×3 copper-tinted colormap. Default n=256.
/// x = (0:n-1)/(n-1); r = min(5/4·x, 1); g = 0.7812·x; b = 0.4975·x.
/// n==1 → [0 0 0]; n ≤ 0 → 0×3.
Value copper_cmap(std::pmr::memory_resource *mr, int n);

/// `map = pink([n])` — N×3 pastel-pink colormap. Default n=256.
/// idx = floor(3/8·n); piecewise linspace ramps for R/G/B then take
/// the element-wise sqrt to lift saturation. n==1 → sqrt([1/3 1/3 1/3]);
/// n==2 → sqrt([1/3 1/3 1/6; 1 1 1]); n ≤ 0 → 0×3.
Value pink_cmap(std::pmr::memory_resource *mr, int n);

/// `map = hsv([n])` — N×3 hue-rotation colormap. Default n=256. Equivalent
/// to `hsv2rgb([(0:n-1)'/n, 1, 1])`. n==1 → [1 0 0]; n ≤ 0 → 0×3.
Value hsv_cmap(std::pmr::memory_resource *mr, int n);

/// `map = flag([n])` — N×3 cyclic red/white/blue/black colormap.
/// Rows cycle through the 4-row pattern `[1 0 0; 1 1 1; 0 0 1; 0 0 0]`.
/// Default n = 256. n ≤ 0 → 0×3.
Value flag_cmap(std::pmr::memory_resource *mr, int n);

/// `map = prism([n])` — N×3 cyclic 6-row rainbow palette
/// `[red, orange, yellow, green, blue, violet]` (`[1 0 0; 1 0.5 0;
/// 1 1 0; 0 1 0; 0 0 1; 2/3 0 1]`). Default n = 256. n ≤ 0 → 0×3.
Value prism_cmap(std::pmr::memory_resource *mr, int n);

/// `map = lines([n])` — N×3 colormap that cycles through MATLAB's
/// default axes color order (R2025b 7-row palette). n==1 → [0 0 1]
/// (MATLAB convention); n ≤ 0 → 0×3. Default n = 256. NB: Octave's
/// default palette differs slightly; we match MATLAB.
Value lines_cmap(std::pmr::memory_resource *mr, int n);

/// `map = bone([n])` — N×3 grayscale-with-blue-tint "bone" colormap.
/// Per Octave's bone.m: x=(0:n-1)/(n-1) and per-channel piecewise
/// linspace ramps with idx_R=floor(3/4·n), idx_G=floor(3/8·n).
/// `base` adjustment depends on mod(n,8) (cases {2,4}, {5,7},
/// otherwise). Default n = 256. n==1 → [1/8 1/8 1/8];
/// n==2 → [1/16 1/8 1/8; 1 1 1]; n ≤ 0 → 0×3.
Value bone_cmap(std::pmr::memory_resource *mr, int n);

/// `map = white([n])` — N×3 all-ones colormap. Default n=256.
/// n ≤ 0 → 0×3.
Value white_cmap(std::pmr::memory_resource *mr, int n);

/// `B = rgb2lin(A)` — sRGB → linear-RGB gamma inverse.
/// Per-element piecewise: |x| < 0.04045 → x/12.92; otherwise
/// sign(x) · ((|x|+0.055)/1.055)^2.4. Negatives are mirrored
/// through the gamma curve. Output class is double if input is
/// double, else single (MATLAB R2025b convention). Only the
/// default "sRGB" colorspace is supported here.
Value rgb2lin(std::pmr::memory_resource *mr, const Value &A);

/// `B = lin2rgb(A)` — linear-RGB → sRGB gamma forward.
/// Per-element piecewise: |x| ≤ 0.0031308 → 12.92·x; otherwise
/// sign(x) · (1.055·|x|^(1/2.4) − 0.055). Inverse of rgb2lin.
/// Same class-promotion rule as rgb2lin (integer in → single out).
Value lin2rgb(std::pmr::memory_resource *mr, const Value &A);

/// `xyzd = xyz2double(xyz)` — convert XYZ image/colormap to double.
/// Following the ICC.1:2001-4 convention for uint16 XYZ:
///     uint16 0 → 0.0,  uint16 32768 → 1.0,  uint16 65535 → 1+32767/32768.
/// Equivalently `double = uint16 / 32768`. Double input is passed through
/// (sanitized to double class). Shape is preserved (M×3 or H×W×3).
Value xyz2double(std::pmr::memory_resource *mr, const Value &xyz);

/// `delE = deltaE(I1, I2[, 'isInputLab', tf])` — CIE76 colour
/// difference. Default treats inputs as RGB and converts to L*a*b*
/// internally; pass `isInputLab=true` to skip the conversion.
/// Result is sqrt(sum((Lab1 - Lab2).^2, 3)) — Euclidean distance
/// in CIELAB. Inputs are M×3 colormaps (output M×1) or H×W×3
/// images (output H×W). Class promotion: any double → double,
/// otherwise single.
Value deltaE(std::pmr::memory_resource *mr,
             const Value &I1, const Value &I2, bool isInputLab);

/// `wp = whitepoint([illuminant])` — 1×3 XYZ tristimulus value of
/// a CIE reference illuminant. Supported (case-insensitive):
///   'a'   → [1.0985 1.0000 0.3558]      (Tungsten 2856 K)
///   'c'   → [0.9807 1.0000 1.1823]      (Average daylight)
///   'd50' → [0.96419866 1.0 0.82511648] (Horizon)
///   'd55' → [0.9568 1.0 0.9214]         (Mid-morning daylight)
///   'd65' → [0.95047 1.0 1.08883]       (Noon daylight)
///   'e'   → [1 1 1]                     (Equal-energy)
///   'icc' → [0.96420288 1.0 0.82489014] (default; ICC profile D50)
Value whitepoint(std::pmr::memory_resource *mr,
                 const std::string &illuminant);

/// `gmap = cmap2gray(cmap)` — colormap → grayscale colormap.
/// Input is an N×3 RGB colormap (treated as double). Output is N×3
/// double, where each row is `[y y y]` and y is the luminance from
/// the inv(YIQ→RGB) first row weights (0.298936, 0.587043, 0.114021),
/// clipped to [0, 1]. Matches MATLAB R2020b+. Octave-image 2.18.2
/// doesn't ship cmap2gray.
Value cmap2gray(std::pmr::memory_resource *mr, const Value &cmap);

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
Value label2rgb(std::pmr::memory_resource *mr,
                const Value &L, const Value &cmap,
                const Value &background);

} // namespace numkit::image
