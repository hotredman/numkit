// libs/signal/include/numkit/signal/filter_implementation/conversions.hpp
//
// Convert between filter representations: zp2sos / tf2sos. The cascade
// applicator sosfilt lives in digital_filtering/sosfilt.hpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Convert zeros / poles / gain → L×6 SOS matrix. Returns the matrix
/// only; the global gain is distributed across the sections (matches
/// MATLAB's default 1-output form).
Value zp2sos(std::pmr::memory_resource *mr, const Value &zeros, const Value &poles, double gain);

/// Convert zeros / poles / gain → (sos, g). The two-output form keeps
/// the gain factored out (sos has no leading scale; final output is
/// g * cascade(sos)).
std::tuple<Value, double>
zp2sosWithGain(std::pmr::memory_resource *mr, const Value &zeros, const Value &poles, double gain);

/// Convert transfer-function (b, a) → SOS matrix. Internally:
/// roots(b) → zeros, roots(a) → poles, gain = b(1) / a(1), then
/// dispatches to zp2sos.
Value tf2sos(std::pmr::memory_resource *mr, const Value &b, const Value &a);

/// Two-output form of tf2sos.
std::tuple<Value, double>
tf2sosWithGain(std::pmr::memory_resource *mr, const Value &b, const Value &a);

} // namespace numkit::signal
