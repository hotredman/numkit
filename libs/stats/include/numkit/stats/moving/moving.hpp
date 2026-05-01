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

namespace numkit::stats {

Value movmean  (std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim = 0);
Value movmedian(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim = 0);
Value movsum   (std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim = 0);
Value movmin   (std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim = 0);
Value movmax   (std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim = 0);
Value movstd   (std::pmr::memory_resource *mr, const Value &x, const Value &k, int normFlag = 0, int dim = 0);
Value movvar   (std::pmr::memory_resource *mr, const Value &x, const Value &k, int normFlag = 0, int dim = 0);
Value movmad   (std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim = 0);
Value movprod  (std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim = 0);

/// smoothdata(x[, method[, k]]) — wrapper over moving stats.
/// method: "movmean" (default), "movmedian", "gaussian", "lowess",
///         "loess", "rlowess", "rloess", "sgolay".
/// Currently supported: movmean, movmedian, gaussian (Gaussian-weighted
/// running mean). Other methods throw m:smoothdata:unsupportedMethod.
/// Without explicit k, picks a heuristic window from the data length.
Value smoothdata(std::pmr::memory_resource *mr, const Value &x,
                 const std::string &method = "movmean",
                 int k = 0, int dim = 0);

/// hampel(x[, k[, nsigmas]]) — Hampel outlier-resilient median filter.
/// Replaces samples that deviate by > nsigmas·MAD from the local median
/// (window 2k+1). Default k=3, nsigmas=3.0.
Value hampel(std::pmr::memory_resource *mr, const Value &x,
             int k = 3, double nsigmas = 3.0);

} // namespace numkit::stats
