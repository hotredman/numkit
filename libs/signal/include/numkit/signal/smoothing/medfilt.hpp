// libs/signal/include/numkit/signal/smoothing/medfilt.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// medfilt1(x, k) — sliding-window median over a window of k samples
/// (default 3). Window is symmetric around the current sample with
/// boundary truncation (no zero padding) — matches MATLAB's
/// 'truncate' endpoint mode, which is the default.
Value medfilt1(Allocator &alloc, const Value &x, size_t k = 3);

} // namespace numkit::signal
