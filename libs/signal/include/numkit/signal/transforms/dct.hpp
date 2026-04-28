// libs/signal/include/numkit/signal/transforms/dct.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// dct(x) — Type-II discrete cosine transform (MATLAB default).
Value dct(Allocator &alloc, const Value &x);

/// idct(x) — inverse Type-II DCT.
Value idct(Allocator &alloc, const Value &x);

} // namespace numkit::signal
