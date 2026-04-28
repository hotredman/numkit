// libs/signal/include/numkit/m/signal/smoothing/medfilt.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// medfilt1(x, k) — sliding-window median over a window of k samples
/// (default 3). Window is symmetric around the current sample with
/// boundary truncation (no zero padding) — matches MATLAB's
/// 'truncate' endpoint mode, which is the default.
MValue medfilt1(Allocator &alloc, const MValue &x, size_t k = 3);

} // namespace numkit::m::signal
