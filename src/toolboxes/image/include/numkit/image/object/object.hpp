// toolboxes/image/include/numkit/image/object/object.hpp
//
// Object analysis: gradient / edge detection.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit::image {

/// Component gradients (`[Gx, Gy] = imgradientxy(I, method)`).
///
/// Computes the horizontal and vertical first-order partial
/// derivatives of `I` using one of:
///
///   | method           | kernel                                  |
///   | ---------------- | --------------------------------------- |
///   | `"sobel"` (def.) | 3×3 Sobel operator                      |
///   | `"prewitt"`      | 3×3 Prewitt                             |
///   | `"central"`      | central difference: `[1 0 -1] / 2`      |
///   | `"intermediate"` | forward difference: `[1 -1]`            |
///
/// Outputs are DOUBLE images of the same H×W as `I`.
///
/// @param I       Input image (any numeric class; cast to double).
/// @param method  Gradient kernel name (case-sensitive).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `(Gx, Gy)`.
///
/// @see imgradient, edge
std::tuple<Value, Value>
imgradientxy(const Value &I, const std::string &method,
             std::pmr::memory_resource *mr = nullptr);

/// Gradient magnitude and direction (`[Gmag, Gdir] = imgradient(I, method)`).
///
/// Equivalent to running @ref imgradientxy and converting to polar:
///   @f$ G_\text{mag} = \sqrt{G_x^2 + G_y^2} @f$,
///   @f$ G_\text{dir} = \text{atan2}(-G_y, G_x) @f$ in degrees
///   (−180 ≤ Gdir ≤ 180; the `-Gy` flip makes the
///   angle increase counter-clockwise in screen coordinates).
///
/// @see imgradientxy, edge
std::tuple<Value, Value>
imgradient(const Value &I, const std::string &method,
           std::pmr::memory_resource *mr = nullptr);

/// 3-D component gradients
/// (`[Gx, Gy, Gz] = imgradientxyz(V, method)`).
///
/// Supports `"sobel"` (default), `"prewitt"`, `"central"`, and
/// `"intermediate"`. The Sobel / Prewitt kernels are the standard
/// 3×3×3 separable extensions used by MATLAB R2025b (Sobel weights
/// `[1, 3, 3, 1]`-style — not the naive `[1, 2, 1]` 2-D extension).
/// `"central"` ≡ `gradient(V)`; `"intermediate"` ≡ forward `diff`
/// with the trailing slice zero-padded.
///
/// Replicate boundary handling for the convolution kernels.
/// Output class is `single` if `V` is `single`, else `double`.
///
/// @param V       3-D grayscale volume.
/// @param method  `"sobel"` / `"prewitt"` / `"central"` /
///                `"intermediate"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(Gx, Gy, Gz)` (same shape as `V`).
std::tuple<Value, Value, Value>
imgradientxyz(const Value &V, const std::string &method,
              std::pmr::memory_resource *mr = nullptr);

/// 3-D gradient magnitude + spherical direction
/// (`[Gmag, Gaz, Gelev] = imgradient3(V, method)` or
/// `(Gx, Gy, Gz)` inputs).
///
/// `Gmag = hypot(hypot(Gx, Gy), Gz)`,
/// `Gaz = atan2(-Gy, Gx)` (degrees),
/// `Gelev = atan2(Gz, hypot(Gx, Gy))` (degrees).
///
/// Single-output form returns only `Gmag`. The `(Gx, Gy, Gz)`
/// alternate form skips the convolution step (used when the
/// caller already has the directional gradients).
///
/// @see imgradientxyz
std::tuple<Value, Value, Value>
imgradient3(const Value &V, const std::string &method,
            std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value, Value>
imgradient3_from_grads(const Value &Gx, const Value &Gy, const Value &Gz,
                       std::pmr::memory_resource *mr = nullptr);

/// Binary edge map (`BW = edge(I, method, thresh_lo, thresh_hi)`).
///
/// Detector dispatch:
///
///   | method        | description                                    |
///   | ------------- | ---------------------------------------------- |
///   | `"sobel"` (d) | gradient magnitude > thresh                    |
///   | `"prewitt"`   | gradient magnitude > thresh                    |
///   | `"roberts"`   | 2×2 cross-difference                           |
///   | `"canny"`     | Canny edge detector (uses both thresholds)     |
///   | `"log"`       | Laplacian of Gaussian, zero crossings ± thresh |
///   | `"zerocross"` | zero crossings of a user kernel (not yet wired)|
///
/// `thresh_lo` / `thresh_hi` accept `NaN` to request the auto-pick
/// heuristic (30 % of max gradient for the simple detectors; Otsu /
/// fraction-based for Canny).
///
/// @param I          Input image.
/// @param method     Detector name.
/// @param thresh_lo  Lower threshold or NaN.
/// @param thresh_hi  Upper threshold (Canny) or NaN.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Logical edge map.
///
/// @see imgradient
Value edge(const Value &I, const std::string &method,
           double thresh_lo, double thresh_hi,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Corner metric matrix (`C = cornermetric(I, method, ...)`).
///
/// Computes per-pixel corner-likelihood using the Harris (default) or
/// Shi–Tomasi (`"MinimumEigenvalue"`) detector. Larger output values
/// indicate stronger corner features.
///
/// Algorithm (Harris 1988, Shi-Tomasi 1994):
///   Dx = imfilter(I, [-1 0 1],  'replicate', 'conv')
///   Dy = imfilter(I, [-1 0 1]', 'replicate', 'conv')
///   trim 1-pixel border on Dx, Dy
///   A = Dx², B = Dy², C = Dx·Dy
///   smooth A, B, C with W = filter_coef · filter_coef'   (full conv)
///   crop back to image size
///   Harris:           cornerness = A·B − C² − k·(A+B)²
///   MinimumEigenvalue: cornerness = ((A+B) − √((A−B)² + 4·C²)) / 2
///
/// Default `filter_coef = fspecial('gaussian', [5 1], 1.5)` =
/// `[0.1201, 0.2339, 0.2921, 0.2339, 0.1201]`. `sensitivity_factor`
/// `k ∈ (0, 0.25)` applies only to the Harris method (default 0.04).
///
/// References:
///   [1] Harris & Stephens, "A Combined Corner and Edge Detector",
///       4th Alvey Vision Conference, 1988.
///   [2] Shi & Tomasi, "Good Features to Track", CVPR 1994.
///
/// Output class is DOUBLE regardless of input class. Image is
/// internally promoted via `im2double` (uint8/uint16/single/etc.).
///
/// @param I                  2-D grayscale or logical image.
/// @param method             `"Harris"` (default) or
///                           `"MinimumEigenvalue"`.
/// @param sensitivity_factor `k ∈ (0, 0.25)`; ignored for MinEig.
/// @param filter_coef        1-D smoothing filter coefficients
///                           (length ≥ 3, odd). Empty → default
///                           5-tap Gaussian.
/// @param mr                 Memory resource (nullptr → process default).
/// @return                   `H × W` DOUBLE corner-metric image.
Value cornermetric(const Value &I, const std::string &method,
                   double sensitivity_factor,
                   const Value &filter_coef,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Detect corner points (`C = corner(I, ...)`).
///
/// Wraps @ref cornermetric: keep its local maxima above
/// `quality_level · max(metric)`, take each connected peak's centroid,
/// sort by strength (descending), and return up to `maxN` integer
/// `[x y] = [col row]` coordinates (one row per corner). Equal-strength
/// peaks tie-break by ascending column-major index (MATLAB `find` order);
/// the image border is excluded naturally (its metric is ≤ 0 < threshold).
///
/// @param I              2-D grayscale or logical image.
/// @param method         `"Harris"` (default) or `"MinimumEigenvalue"`.
/// @param maxN           Maximum number of corners (default 200; <0 = all).
/// @param quality_level  Threshold fraction of the peak metric (default 0.01).
/// @param sensitivity    Harris `k ∈ (0, 0.25)` (default 0.04); ignored for MinEig.
/// @param filter_coef    Smoothing filter (empty → default 5-tap Gaussian).
/// @param mr             Memory resource (nullptr → process default).
/// @return               `K × 2` DOUBLE matrix of `[x y]` corner coordinates.
/// @see cornermetric
Value corner(const Value &I, const std::string &method, int maxN,
             double quality_level, double sensitivity,
             const Value &filter_coef,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Standard Hough transform (`[H, T, R] = hough(BW, ...)`).
///
/// Computes the parameter-space accumulator for the line
/// @f$ \rho = x \cos\theta + y \sin\theta @f$ over the foreground
/// pixels of binary image `BW`. Each true pixel votes for every
/// `(θ, ρ)` bin on its sinusoid; peaks in `H` represent collinear
/// pixels.
///
/// Default `θ` grid is `-90 : 1 : 89` degrees (180 bins). Default
/// `ρ` grid is `linspace(-D, D, 2·⌈D/rhoRes⌉+1)` where
/// `D = √((M-1)² + (N-1)²)`. Custom `theta` (any vector in
/// `[-90, 90)`) and `rhoRes` (positive scalar) override the
/// defaults.
///
/// Reference: Gonzalez, Woods & Eddins, *Digital Image Processing
/// Using MATLAB*, 2nd ed., Gatesmark, 2009.
///
/// @param BW          2-D binary mask (logical; numeric is cast).
/// @param rho_res     Bin spacing along `ρ`. Default 1.
/// @param theta_deg   Theta grid in degrees (any vector in
///                    `[-90, 90)`). Empty → default `-90:1:89`.
/// @param[out] H_out  Accumulator (`nRho × nTheta` DOUBLE).
/// @param[out] T_out  θ vector echoed (1 × nTheta).
/// @param[out] R_out  ρ vector (1 × nRho).
/// @param mr          Memory resource (nullptr → process default).
void hough(const Value &BW, double rho_res,
           const Value &theta_deg,
           Value &H_out, Value &T_out, Value &R_out,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Find peaks in a Hough-transform accumulator
/// (`P = houghpeaks(H, NumPeaks, ...)`).
///
/// Iteratively picks the global maximum of `H`, records its
/// `[rho_idx, theta_idx]`, then zeroes out a rectangular
/// neighbourhood of size `NHoodSize` around it. Stops when either
/// `NumPeaks` peaks are found or the current max falls below
/// `threshold` (default `0.5·max(H(:))`).
///
/// When `theta` covers a full antisymmetric range
/// `[-90, 90)`, the neighbourhood suppression wraps across the
/// θ edge with rho flip (so a peak near θ = ±90° also suppresses
/// the equivalent peak on the opposite side).
///
/// `NHoodSize` default: smallest odd integers ≥ `size(H) / 50`,
/// minimum `[1 1]`.
///
/// @param H         `nRho × nTheta` Hough accumulator from @ref hough.
/// @param numpeaks  Max number of peaks to return.
/// @param threshold Minimum accumulator value (negative → use
///                  default `0.5·max(H(:))`).
/// @param nhoodRho  Suppression neighbourhood rows (odd).
///                  Pass 0 → use default.
/// @param nhoodTheta Suppression neighbourhood cols (odd).
///                  Pass 0 → use default.
/// @param theta_deg Theta vector used by @ref hough (empty →
///                  `-90:1:89`, suppression wraps over `[-90, 90)`).
/// @param mr        Memory resource (nullptr → process default).
/// @return          `P × 2` DOUBLE matrix; each row `[rho_idx,
///                  theta_idx]` is 1-based into `H`.
Value houghpeaks(const Value &H, std::size_t numpeaks,
                 double threshold,
                 std::size_t nhoodRho, std::size_t nhoodTheta,
                 const Value &theta_deg,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Extract line segments from Hough-transform peaks
/// (`lines = houghlines(BW, theta, rho, peaks, ...)`).
///
/// Walks each peak from @ref houghpeaks, finds the nonzero pixels of
/// `BW` that map to that bin, sorts them along the line direction,
/// then splits the run into segments wherever consecutive pixels are
/// farther than `FillGap` apart. Segments shorter than `MinLength`
/// are discarded.
///
/// Returned struct-array fields per segment:
///   * `point1`  — `[x1, y1]` endpoint (1-based image coords).
///   * `point2`  — `[x2, y2]` endpoint.
///   * `theta`   — bin angle in degrees.
///   * `rho`     — bin distance.
///
/// Reference: Gonzalez/Woods/Eddins, *Digital Image Processing
/// Using MATLAB*, Prentice Hall 2003.
///
/// @param BW         2-D binary image (logical or numeric).
/// @param theta_deg  θ vector from @ref hough.
/// @param rho        ρ vector from @ref hough.
/// @param peaks      `P × 2` index matrix from @ref houghpeaks.
/// @param fillgap    Merge consecutive sub-segments within this
///                   pixel distance. Default 20.
/// @param minlength  Discard segments shorter than this. Default 40.
/// @param mr         Memory resource (nullptr → process default).
/// @return           `1 × N` struct array (possibly empty) with the
///                   four fields above.
Value houghlines(const Value &BW, const Value &theta_deg,
                 const Value &rho, const Value &peaks,
                 double fillgap, double minlength,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
