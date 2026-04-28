// libs/signal/include/numkit/signal/transforms/goertzel.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// goertzel(x, ind) — single-frequency DFT for each 1-based index in
/// `ind`. Output is complex, same shape as `ind`.
Value goertzel(Allocator &alloc, const Value &x, const Value &ind);

} // namespace numkit::signal
