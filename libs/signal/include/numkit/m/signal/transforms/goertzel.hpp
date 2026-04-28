// libs/signal/include/numkit/m/signal/transforms/goertzel.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// goertzel(x, ind) — single-frequency DFT for each 1-based index in
/// `ind`. Output is complex, same shape as `ind`.
MValue goertzel(Allocator &alloc, const MValue &x, const MValue &ind);

} // namespace numkit::m::signal
