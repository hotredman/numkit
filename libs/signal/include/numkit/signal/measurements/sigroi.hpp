// libs/signal/include/numkit/signal/measurements/sigroi.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <cstdint>

namespace numkit::signal {

/// binmask2sigroi(m) — convert a binary mask to ROI [start, end] pairs.
/// Returns N×2 matrix where each row is a 1-based inclusive run of true
/// values in `m`. Empty mask → 0×0 matrix.
Value binmask2sigroi(const Value &                m,
                     std::pmr::memory_resource *  mr = nullptr);

/// sigroi2binmask(roi[, len]) — convert ROIs back to a logical column.
/// `len`=-1 (default) → length = max(roi[:,1]); otherwise pad/clamp.
Value sigroi2binmask(const Value &                roi,
                     int64_t                      len = -1,
                     std::pmr::memory_resource *  mr  = nullptr);

/// extendsigroi(roi, Lpre, Lpost) — extend each ROI by Lpre samples on
/// the left and Lpost samples on the right (clamping start to ≥ 1).
Value extendsigroi(const Value &                roi,
                   int64_t                      Lpre,
                   int64_t                      Lpost,
                   std::pmr::memory_resource *  mr = nullptr);

/// shortensigroi(roi, Lpre, Lpost) — shrink each ROI by Lpre on the
/// left and Lpost on the right. ROIs that collapse (end < start) are
/// dropped.
Value shortensigroi(const Value &                roi,
                    int64_t                      Lpre,
                    int64_t                      Lpost,
                    std::pmr::memory_resource *  mr = nullptr);

/// mergesigroi(roi, sep) — merge ROIs separated by at most `sep` samples.
/// sep=0 → only strictly overlapping ROIs merge; sep≥1 → near-adjacent.
Value mergesigroi(const Value &                roi,
                  int64_t                      sep,
                  std::pmr::memory_resource *  mr = nullptr);

/// removesigroi(roi, maxLen) — drop ROIs whose length is ≤ maxLen.
Value removesigroi(const Value &                roi,
                   int64_t                      maxLen,
                   std::pmr::memory_resource *  mr = nullptr);

/// extractsigroi(x, roi[, concat]) — extract signal slices.
///   concat=false (default): cell-array of column-vector slices.
///   concat=true: single concatenated column vector.
Value extractsigroi(const Value &                x,
                    const Value &                roi,
                    bool                         concat = false,
                    std::pmr::memory_resource *  mr     = nullptr);

/// sigrangebinmask(x, bound) — logical mask where x is in `bound`:
///   - bound scalar → mask where x > bound
///   - bound 2-vector [lo, hi] → mask where lo ≤ x ≤ hi
Value sigrangebinmask(const Value &                x,
                      const Value &                bound,
                      std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
