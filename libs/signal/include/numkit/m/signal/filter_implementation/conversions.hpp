// libs/signal/include/numkit/m/signal/filter_implementation/conversions.hpp
//
// Convert between filter representations: zp2sos / tf2sos. The cascade
// applicator sosfilt lives in digital_filtering/sosfilt.hpp.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::signal {

/// Convert zeros / poles / gain → L×6 SOS matrix. Returns the matrix
/// only; the global gain is distributed across the sections (matches
/// MATLAB's default 1-output form).
MValue zp2sos(Allocator &alloc, const MValue &zeros, const MValue &poles, double gain);

/// Convert zeros / poles / gain → (sos, g). The two-output form keeps
/// the gain factored out (sos has no leading scale; final output is
/// g * cascade(sos)).
std::tuple<MValue, double>
zp2sosWithGain(Allocator &alloc, const MValue &zeros, const MValue &poles, double gain);

/// Convert transfer-function (b, a) → SOS matrix. Internally:
/// roots(b) → zeros, roots(a) → poles, gain = b(1) / a(1), then
/// dispatches to zp2sos.
MValue tf2sos(Allocator &alloc, const MValue &b, const MValue &a);

/// Two-output form of tf2sos.
std::tuple<MValue, double>
tf2sosWithGain(Allocator &alloc, const MValue &b, const MValue &a);

} // namespace numkit::m::signal
