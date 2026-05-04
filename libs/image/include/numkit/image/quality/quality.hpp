// libs/image/include/numkit/image/quality/quality.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// immse(A, B) — mean squared error.
Value immse(std::pmr::memory_resource *mr, const Value &A, const Value &B);

/// psnr(A, B[, peak]) — peak SNR. peak defaults to class max
/// (255 for uint8, 65535 for uint16, 1.0 for floats).
Value psnr(std::pmr::memory_resource *mr, const Value &A, const Value &B, double peak);

/// ssim(A, B) — Wang-Bovik structural similarity. Uses a fixed 11×11
/// Gaussian window (σ=1.5) with K1=0.01, K2=0.03 — matches MATLAB defaults.
Value ssim(std::pmr::memory_resource *mr, const Value &A, const Value &B);

/// corr2(A, B) — 2-D correlation coefficient (Pearson r over all
/// elements). A and B must have the same number of elements; class is
/// converted to double for the computation.
Value corr2(std::pmr::memory_resource *mr, const Value &A, const Value &B);

/// mean2(A) — mean of all elements (2-D semantics ignores higher
/// dims by treating the array as flat).
Value mean2(std::pmr::memory_resource *mr, const Value &A);

/// std2(A) — standard deviation of all elements (normalized by N-1).
Value std2(std::pmr::memory_resource *mr, const Value &A);

} // namespace numkit::image
