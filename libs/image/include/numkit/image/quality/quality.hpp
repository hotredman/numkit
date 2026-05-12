// libs/image/include/numkit/image/quality/quality.hpp
//
// Image-quality metrics.

#pragma once

#include <memory_resource>
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
/// and constants `K1 = 0.01`, `K2 = 0.03`. Matches MATLAB R2025b
/// defaults. Output is a real scalar in roughly `[-1, 1]` (1 = identical).
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
/// Normalised by `N - 1` (sample standard deviation), matching
/// MATLAB's `std2`.
///
/// @param A   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar standard deviation.
/// @see mean2
Value std2(const Value &A, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
