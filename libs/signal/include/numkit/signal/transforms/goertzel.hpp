// libs/signal/include/numkit/signal/transforms/goertzel.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// goertzel(x, ind) — single-frequency DFT for each 1-based index in
/// `ind`. Output is complex, same shape as `ind`.
Value goertzel(std::pmr::memory_resource *mr, const Value &x, const Value &ind);

} // namespace numkit::signal
