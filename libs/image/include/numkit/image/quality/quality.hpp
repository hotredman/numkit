// libs/image/include/numkit/image/quality/quality.hpp
//
// Image-quality metrics.

#pragma once

#include <memory_resource>
#include <vector>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// @brief Mean squared error (`err = immse(A, B)`).
///
/// @f$ \text{MSE} = \dfrac{1}{N}\sum_i (A_i - B_i)^2 @f$.
///
/// @param A   First image.
/// @param B   Second image (same shape as `A`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Real scalar MSE.
/// @see psnr, ssim
Value immse(const Value &A, const Value &B,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Peak signal-to-noise ratio (`r = psnr(A, B, peak)`).
///
/// @f$ \text{PSNR} = 10\log_{10}\!\left(\dfrac{\text{peak}^2}{\text{MSE}(A, B)}\right) @f$
/// in dB. `peak` defaults to the class maximum: 255 for UINT8, 65535
/// for UINT16, 1.0 for floating point. Pass a positive value to override.
///
/// @param A     First image.
/// @param B     Second image (same shape as `A`).
/// @param peak  Peak intensity (positive; 0 → class default).
/// @param mr    Memory resource (nullptr → process default).
/// @return      PSNR in dB. Returns `+Inf` when MSE = 0.
/// @see immse, ssim
Value psnr(const Value &A, const Value &B, double peak,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Structural similarity index (`r = ssim(A, B)`).
///
/// Wang-Bovik SSIM with a fixed `11 × 11` Gaussian window (σ = 1.5)
/// and constants `K1 = 0.01`, `K2 = 0.03`. Output is a real scalar
/// in roughly `[-1, 1]` (1 = identical).
///
/// @param A   First image.
/// @param B   Second image (same shape).
/// @param mr  Memory resource (nullptr → process default).
/// @return    SSIM scalar.
/// @see immse, psnr
Value ssim(const Value &A, const Value &B,
           std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D correlation coefficient (`r = corr2(A, B)`).
///
/// Pearson `r` computed over all elements (flattened). Class is
/// converted to DOUBLE for the computation; `A` and `B` must have the
/// same `numel`.
///
/// @param A   First image.
/// @param B   Second image (same `numel`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Pearson correlation scalar.
Value corr2(const Value &A, const Value &B,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Mean of every element (`m = mean2(A)`).
///
/// 2-D semantics: higher-dimensional inputs are treated as flat and
/// averaged element-wise.
///
/// @param A   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar mean.
/// @see std2
Value mean2(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Standard deviation of every element (`s = std2(A)`).
///
/// Normalised by `N - 1` (sample standard deviation).
///
/// @param A   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar standard deviation.
/// @see mean2
Value std2(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Multi-scale structural similarity index
/// (`[score, qmaps] = multissim(I, Iref, ...)`).
///
/// Wang-Simoncelli-Bovik 2003 multi-scale SSIM, "Asilomar Conf. on
/// Signals, Systems & Computers". Generalizes SSIM by averaging
/// the contrast-and-structural component across a Gaussian pyramid
/// and applying the full SSIM formula only at the coarsest scale.
///
/// Algorithm:
///
///   1. For each scale `i = 1 .. numScales - 1`:
///      - `ssimmap_i = (2·σxy + C₂) / (σx² + σy² + C₂)` (no luminance).
///      - Mean over `ssimmap_i`, clamped to ≤ 1, raised to
///        `scaleWeights[i]` (negative bases clamped to 0).
///      - Lowpass with `[1 1; 1 1] / 4` (replicate boundary).
///      - Downsample by 2 (take rows/cols 1:2:end).
///   2. At the coarsest scale, compute the full SSIM with luminance:
///      `(2·μx·μy + C₁)·(2·σxy + C₂) /
///       ((μx² + μy² + C₁)·(σx² + σy² + C₂))`.
///   3. `score = prod_i (mean(ssimmap_i)^scaleWeights[i])`.
///
/// Local statistics use an `N × N` isotropic Gaussian filter with
/// standard deviation `sigma` and `N = 2·ceil(3·sigma) + 1` (so
/// `≥ 99.7%` of the kernel mass is included). Replicate boundary.
///
/// `C₁ = (0.01·L)²`, `C₂ = (0.03·L)²`, where `L` is the dynamic
/// range (255 for uint8, 65535 for uint16/int16, 1 for single /
/// double). `dynamic_range = -1.0` activates the class default.
///
/// `scaleWeights` defaults to `fspecial('gaussian', [1 numScales], 1)`
/// = a length-`numScales` Gaussian sample then normalised to sum to 1.
/// Pass an empty `std::vector` to get the default; otherwise the
/// vector is normalised internally to sum to 1.
///
/// Class support: `uint8`/`uint16`/`int16`/`single`/`double`. Image
/// classes are promoted to SINGLE internally except `double`, which
/// is computed in double. Output is SINGLE for single inputs,
/// DOUBLE for double, otherwise SINGLE.
///
/// When `quality_maps_out` is non-null, the per-scale quality maps
/// are written to it (length `numScales`, with `qmaps[i]` sized to
/// match the i-th downsampled image).
///
/// @param A                 First image (grayscale `H × W`).
/// @param Iref              Reference image (same class and size).
/// @param num_scales        Pyramid depth (default 5).
/// @param scale_weights     Per-scale weights, length = num_scales.
///                          Empty → default Gaussian weights.
/// @param sigma             Gaussian filter σ (default 1.5).
/// @param dynamic_range     `L` (-1.0 = class default).
/// @param quality_maps_out  Optional output: per-scale `ssimmap`.
/// @param mr                Memory resource (nullptr → process default).
/// @return                  Scalar (SINGLE / DOUBLE) MS-SSIM score in
///                          `(-1, 1]` (1 = identical).
Value multissim(const Value &A, const Value &Iref,
                int num_scales,
                const std::vector<double> &scale_weights,
                double sigma, double dynamic_range,
                std::vector<Value> *quality_maps_out,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
