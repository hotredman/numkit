// libs/signal/include/numkit/signal/digital_filtering/buffer.hpp
//
// MATLAB R2025b Signal Toolbox `buffer` — partition a signal into
// (possibly overlapping or underlapping) frames.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>
#include <tuple>

namespace numkit::signal {

// buffer(x, n)            — non-overlapping frames of length n, last frame zero-padded
// buffer(x, n, p)         — p > 0: overlap with p initial zeros; p < 0: skip |p|/frame
// buffer(x, n, p, opt)    — for p > 0, opt='nodelay' removes initial zeros;
//                            for p < 0, opt = numeric initial offset to skip [0..-p]
//
// Returns Y only. Use buffer2() for the [Y, Z] form which separates
// complete frames from partial trailing samples.
Value buffer(std::pmr::memory_resource *mr,
              const Value &x, int n, int p = 0,
              const Value *opt = nullptr);

// 2-output form: [Y, Z] = buffer(...). Y has only complete frames (no
// trailing zero-pad). Z has the partial-frame remainder (orientation
// matches X). Returns (Y, Z).
std::tuple<Value, Value>
buffer2(std::pmr::memory_resource *mr,
         const Value &x, int n, int p = 0,
         const Value *opt = nullptr);

} // namespace numkit::signal
