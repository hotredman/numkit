// libs/image/include/numkit/image/quality/quality.hpp
//
// Image-quality metrics: similarity / error between two same-sized
// images, plus 2-D summary statistics.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// Mean squared error between two images (`err = immse(A, B)`).
///
/// @f$ \text{MSE} = \frac{1}{N}\sum_i (A_i - B_i)^2 @f$.
///
/// @param A,B  Same-sized images.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Real scalar Value.
///
/// @see psnr, ssim
Value immse(const Value &A, const Value &B,
            std::pmr::memory_resource *mr = nullptr);

/// Peak signal-to-noise ratio (`r = psnr(A, B, peak)`).
///
/// @f$ \text{PSNR} = 10\log_{10}\!\left(\frac{\text{peak}^2}{\text{MSE}(A,B)}\right) @f$
/// in dB. `peak` defaults to the class maximum: 255 for uint8,
/// 65535 for uint16, 1.0 for floating point. Pass a positive value
/// to override.
///
/// @param A,B   Same-sized images.
/// @param peak  Peak intensity (positive; 0 → use class default).
/// @param mr    Memory resource (nullptr → process default).
/// @return      PSNR in dB. Returns +Inf when MSE = 0.
///
/// @see immse, ssim
Value psnr(const Value &A, const Value &B, double peak,
           std::pmr::memory_resource *mr = nullptr);

/// Structural similarity index (`r = ssim(A, B)`).
///
/// Wang–Bovik SSIM with a fixed 11×11 Gaussian window (σ = 1.5)
/// and constants `K1 = 0.01`, `K2 = 0.03`. Matches MATLAB R2025b
/// defaults. Output is a real scalar in roughly [-1, 1] (1 = identical).
///
/// @see immse, psnr
Value ssim(const Value &A, const Value &B,
           std::pmr::memory_resource *mr = nullptr);

/// 2-D correlation coefficient (`r = corr2(A, B)`).
///
/// Pearson @f$ r @f$ computed over all elements of `A` and `B`
/// (flattened). Class is converted to double for the computation;
/// `A` and `B` must have the same `numel`.
Value corr2(const Value &A, const Value &B,
            std::pmr::memory_resource *mr = nullptr);

/// Mean of every element (`m = mean2(A)`).
///
/// 2-D semantics: higher-dimensional inputs are treated as flat and
/// averaged element-wise.
Value mean2(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Standard deviation of every element (`s = std2(A)`).
///
/// Normalised by `N − 1` (sample standard deviation), matching
/// MATLAB's `std2`.
Value std2(const Value &A, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
