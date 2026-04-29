// libs/signal/include/numkit/signal/smoothing/medfilt.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// medfilt1(x, k) — sliding-window median over a window of k samples
/// (default 3). Window is symmetric around the current sample with
/// boundary truncation (no zero padding) — matches MATLAB's
/// 'truncate' endpoint mode, which is the default.
Value medfilt1(std::pmr::memory_resource *mr, const Value &x, size_t k = 3);

} // namespace numkit::signal
