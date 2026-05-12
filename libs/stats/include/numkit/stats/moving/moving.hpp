// libs/stats/include/numkit/stats/moving/moving.hpp
//
// Moving / sliding-window statistics. All functions accept either:
//   * scalar window length k → centred window of length k
//     (kb = floor((k-1)/2) leading samples, kf = floor(k/2) trailing)
//   * 2-vector [kb kf] → asymmetric window [i-kb, i+kf]
//
// Endpoints are NOT discarded — the window truncates ('shrink' is the
// MATLAB default). Windows that are empty after truncation produce NaN.
//
// Optional `dim` argument follows the rest of stats:
//   dim == 0  → first non-singleton dim (auto)
//   dim >= 1  → reduce along that dim. Vector / scalar input ignores dim.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::stats {

/// Moving mean (`y = movmean(x, k)`). Window `k` interpretation: see
/// the file header.
Value movmean(const Value &x, const Value &k, int dim = 0,
              std::pmr::memory_resource *mr = nullptr);

/// Moving median (`y = movmedian(x, k)`).
Value movmedian(const Value &x, const Value &k, int dim = 0,
                std::pmr::memory_resource *mr = nullptr);

/// Moving sum (`y = movsum(x, k)`).
Value movsum(const Value &x, const Value &k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// Moving minimum (`y = movmin(x, k)`).
Value movmin(const Value &x, const Value &k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// Moving maximum (`y = movmax(x, k)`).
Value movmax(const Value &x, const Value &k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// Moving standard deviation (`y = movstd(x, k, normFlag)`).
///
/// `normFlag = 0` (default) divides by `n-1` (unbiased), `1` divides
/// by `n` (population).
Value movstd(const Value &x, const Value &k, int normFlag = 0, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// Moving variance (`y = movvar(x, k, normFlag)`) — same `normFlag` as
/// @ref movstd.
Value movvar(const Value &x, const Value &k, int normFlag = 0, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// Moving mean absolute deviation (`y = movmad(x, k)`).
Value movmad(const Value &x, const Value &k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// Moving product (`y = movprod(x, k)`).
Value movprod(const Value &x, const Value &k, int dim = 0,
              std::pmr::memory_resource *mr = nullptr);

/// Smoothing dispatcher (`y = smoothdata(x, method, k)`).
///
/// Supported `method`: `"movmean"` (default), `"movmedian"`,
/// `"gaussian"` (Gaussian-weighted running mean). Other MATLAB
/// methods (`"lowess"`, `"loess"`, `"rlowess"`, `"rloess"`,
/// `"sgolay"`) throw `m:smoothdata:unsupportedMethod`.
///
/// Without explicit `k`, picks a heuristic window from the data length.
Value smoothdata(const Value &x, const std::string &method = "movmean",
                 int k = 0, int dim = 0,
                 std::pmr::memory_resource *mr = nullptr);

/// Hampel outlier-resilient median filter (`y = hampel(x, k, nsigmas)`).
///
/// Replaces samples that deviate by more than `nsigmas · MAD` from the
/// local median over a window of `2k+1` samples. Defaults: `k = 3`,
/// `nsigmas = 3.0`.
Value hampel(const Value &x, int k = 3, double nsigmas = 3.0,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
