// libs/signal/include/numkit/m/signal/transforms/dct.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// dct(x) — Type-II discrete cosine transform (MATLAB default).
MValue dct(Allocator &alloc, const MValue &x);

/// idct(x) — inverse Type-II DCT.
MValue idct(Allocator &alloc, const MValue &x);

} // namespace numkit::m::signal
