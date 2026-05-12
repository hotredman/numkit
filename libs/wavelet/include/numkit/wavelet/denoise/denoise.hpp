// libs/wavelet/include/numkit/wavelet/denoise/denoise.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::wavelet {

/// `wthresh(X, sorh, T)` — element-wise wavelet thresholding.
///   sorh = "h" : hard,  Y = X .* (|X| > T)
///   sorh = "s" : soft,  Y = sign(X) .* max(|X| - T, 0)
/// Shape and class of X are preserved (always returned as DOUBLE).
Value wthresh(const Value &X, const std::string &sorh, double T, std::pmr::memory_resource *mr = nullptr);

/// `wnoisest(C, L, S)` — noise σ estimator per level via the MAD rule
///   σ̂ = median(|cD_k|) / 0.6745
/// `S` is a vector of 1-based detail levels (1=finest). Returns a row
/// of the same length as S.
Value wnoisest(const Value &C, const Value &L, const Value &S, std::pmr::memory_resource *mr = nullptr);

/// `wdenoise(x [, level [, wname]])` — soft-threshold denoising.
/// Defaults: level = min(floor(log2(N)), 5), wname = "sym4".
/// Pipeline:
///   1. [C, L] = wavedec(x, level, wname)
///   2. σ̂ = median(|cD_1|) / 0.6745
///   3. T = σ̂ · sqrt(2 · ln(N))   (universal / VisuShrink threshold)
///   4. Apply soft-threshold to every detail band; leave cA untouched.
///   5. waverec(C', L, wname)
Value wdenoise(const Value &x, int level, const std::string &wname, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
