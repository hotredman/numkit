// libs/signal/include/numkit/signal/transforms/dct.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// dct(x) — Type-II discrete cosine transform (MATLAB default).
Value dct(std::pmr::memory_resource *mr, const Value &x);

/// idct(x) — inverse Type-II DCT.
Value idct(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::signal
